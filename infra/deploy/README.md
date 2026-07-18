# Deploying apps/api and apps/web to the Radxa

Per ADR 0007 (refining ADR 0006): both run as native Node processes under
systemd on the Radxa (192.168.1.44), not Docker — avoids cross-compiling a
pnpm monorepo for ARM64, and a systemd unit gives the same always-on/auto-
restart guarantee with far less moving parts. Docker remains right for
Mosquitto (a self-contained official image, no build step).

## One-time setup (already done on the current Radxa)

1. SSH key auth to `radxa@192.168.1.44` (see `docs/architecture/adr/0006-radxa-always-on-host.md`).
2. `pnpm` installed globally on the Radxa, symlinked into `/usr/local/bin`
   (Node itself lives at `/opt/node22`, not on PATH by default for
   globally-installed packages — see that ADR for the exact symlink fix
   if setting up a new device).
3. Source deployed to `~/eggapp-app/` — see `deploy-api.sh` / `deploy-web.sh`
   for exactly what each ships. Env files created directly on the device,
   never copied from a dev machine's `.env`:
   - `apps/api/.env` — production secrets. `MQTT_URL` in particular must be
     `mqtt://localhost:1883`, not the LAN IP, since the broker and API are
     co-located.
   - `apps/web/.env.production` — just `NEXT_PUBLIC_API_URL=http://192.168.1.44:3001`.
     Not a secret (it ends up in the client-side JS bundle regardless), but
     kept device-local anyway for consistency and because `next build`
     bakes it in at build time — a dev machine's value could silently drift
     from what's actually deployed.
4. Systemd units (`infra/systemd/eggapp-api.service`, `eggapp-web.service`)
   copied to `/etc/systemd/system/`, then for each:
   `sudo systemctl daemon-reload && sudo systemctl enable --now <unit>`.
5. Narrow passwordless-sudo rules in `/etc/sudoers.d/` (`eggapp-deploy`,
   `eggapp-web-deploy`) scoped to exactly `systemctl restart/status
   <service>` — lets redeploys restart the service without a password
   prompt, without granting broader sudo access. Add analogously for any
   future service.

**`apps/web`'s ExecStart quirk** (documented in the unit file too, but
worth restating): it calls `node_modules/.bin/next` directly as an
executable, not via `pnpm start` and not via `node <path>`. Both of those
fail:
- `pnpm start -- -p 3000 -H 0.0.0.0` → pnpm's `--` forwarding preserves a
  literal `--` in the script's command line, which Next's CLI then treats
  as an end-of-options marker — `-p` becomes a positional arg (parsed as
  the project directory) instead of a flag.
- `node node_modules/.bin/next ...` → that file is pnpm's shell-script
  shim (`#!/bin/sh`), not JavaScript; node fails trying to parse it as JS.

The working form runs the shim directly as an executable (it has a
shebang and the exec bit set):
`/home/radxa/eggapp-app/apps/web/node_modules/.bin/next start -p 3000 -H 0.0.0.0`.

## Redeploying after a code change

```
bash infra/deploy/deploy-api.sh
bash infra/deploy/deploy-web.sh
```

Each packs its app + the workspace packages it depends on, ships them,
rebuilds in place, restarts the service. Run from a real terminal — the
restart step needs a live TTY the first time in a session for `ssh -t`,
though with the NOPASSWD sudoers rules in place it won't actually prompt
for a password.

**If an apps/api change includes a schema migration**, run it yourself first:
```
pnpm --filter @eggapp/db db:deploy
```
(from the dev machine, against the same Supabase database — migrations
don't need to run on the Radxa itself, just once against the shared DB.)

## Operating it

- Logs: `ssh radxa@192.168.1.44 "journalctl -u eggapp-api -f"` /
  `"journalctl -u eggapp-web -f"` — no sudo needed, the `radxa` user is in
  the `adm`/`systemd-journal` groups.
- Status: `ssh radxa@192.168.1.44 "sudo systemctl status eggapp-api"` (or
  `eggapp-web`).
- Both survive reboots (`enabled`) and crashes (`Restart=always`, 5s
  backoff) — verified for `eggapp-api` by force-killing the process and
  confirming systemd respawned it within seconds.
- Reachable on the LAN at `http://192.168.1.44:3001` (API) and
  `http://192.168.1.44:3000` (web dashboard). Not reachable outside the
  home network yet — that's a separate, not-yet-done step (Tailscale or
  similar).
