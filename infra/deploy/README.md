# Deploying apps/api and apps/web to nila

Per ADR 0007 (refining ADR 0006, revised 2026-08-15 when nila replaced the
Radxa as the always-on host): both run as native Node processes under
systemd on nila (Tailscale `100.76.190.23`), not Docker — avoids
cross-compiling a pnpm monorepo for ARM64, and a systemd unit gives the
same always-on/auto-restart guarantee with far less moving parts. Docker
remains right for Mosquitto (a self-contained official image, no build
step).

## One-time setup (already done on the current nila host)

1. SSH key auth to `nila@100.76.190.23` (see
   `docs/architecture/adr/0006-radxa-always-on-host.md` for the general
   pattern this follows).
2. Node 22 and pnpm already installed on nila at `/usr/local/bin/`.
3. Source deployed to `~/eggapp-app/` — see `deploy-api.sh` / `deploy-web.sh`
   for exactly what each ships. Env files created directly on the device,
   never copied from a dev machine's `.env`, with two exceptions treated
   as shared external credentials rather than per-host secrets:
   - `apps/api/.env` — `DATABASE_URL`/`DIRECT_URL` reused from the same
     Supabase project (not per-host); `JWT_SECRET` and
     `MQTT_API_PASSWORD` generated fresh directly on nila.
     `MQTT_URL` is `mqtt://localhost:1883` since the broker and API are
     co-located.
   - `apps/api/firebase-service-account.json` — copied as-is (same
     Firebase project).
   - `apps/web/.env.production` — `NEXT_PUBLIC_API_URL=http://100.76.190.23:3001`
     (Tailscale address, not LAN — this is what makes the dashboard
     reachable off the home network, the gap the Radxa setup never
     closed). Not a secret (it ends up in the client-side JS bundle
     regardless), but kept device-local anyway for consistency and
     because `next build` bakes it in at build time.
4. Systemd units (`infra/systemd/eggapp-api.service`, `eggapp-web.service`)
   copied to `/etc/systemd/system/`, then for each:
   `sudo systemctl daemon-reload && sudo systemctl enable --now <unit>`.
5. Mosquitto (`infra/docker/docker-compose.yml`) — the `passwd` file must
   be world-readable (`chmod 644`) even though it's root-owned; the
   container runs mosquitto as a non-root user and silently exits
   (code 13, no error logged unless `log_type error` is set) if it can't
   read a `600` file. Learned the hard way setting this up on nila.
6. Sudo: nila's `nila` user has passwordless `ALL:ALL` sudo (broader than
   the Radxa's narrow scoped rule) — a deliberate choice to keep setup
   simple; revisit if nila is ever exposed beyond Tailscale.

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
`/home/nila/eggapp-app/apps/web/node_modules/.bin/next start -p 3000 -H 0.0.0.0`.

## Redeploying after a code change

```
bash infra/deploy/deploy-api.sh
bash infra/deploy/deploy-web.sh
```

Each packs its app + the workspace packages it depends on, ships them,
rebuilds in place, restarts the service.

Unlike the old Radxa setup, nila's passwordless `ALL:ALL` sudo means the
restart step does **not** need an interactive TTY or a human at the
keyboard — confirmed working via a plain non-interactive `ssh` restart.
Still worth confirming with `systemctl status` after a deploy rather than
trusting silent success.

**If an apps/api change includes a schema migration**, run it yourself first:
```
pnpm --filter @eggapp/db db:deploy
```
(from the dev machine, against the same Supabase database — migrations
don't need to run on nila itself, just once against the shared DB.)

## Operating it

- Logs: `ssh nila@100.76.190.23 "sudo journalctl -u eggapp-api -f"` /
  `"sudo journalctl -u eggapp-web -f"`.
- Status: `ssh nila@100.76.190.23 "sudo systemctl status eggapp-api"` (or
  `eggapp-web`).
- Both survive reboots (`enabled`) and crashes (`Restart=always`, 5s
  backoff).
- Reachable over Tailscale at `http://100.76.190.23:3001` (API) and
  `http://100.76.190.23:3000` (web dashboard) — from any device with
  Tailscale connected, not just the home LAN. Also reachable on the LAN
  at `http://192.168.1.41:<port>`.

## Migrating from the Radxa (2026-08-15)

The previous host (`radxa@192.168.1.44` / Tailscale `100.92.177.99`) was
decommissioned and is no longer reachable. Its secrets (`apps/api/.env`,
mosquitto's `passwd` file) could not be recovered, so nila runs with
fresh `JWT_SECRET` and MQTT credentials — any existing device MQTT
credentials tied to the old broker will need to be reprogrammed against
nila's new `passwd` file (currently only `api-ingest` is provisioned; see
`infra/docker/mosquitto/README.md` for adding per-device accounts). Still
open, tracked as follow-up work for iot-integration-architect:
- Android app's `API_BASE_URL` needs updating from `100.92.177.99` to
  `100.100.38.32` (`apps/android/README.md` / `build.gradle.kts`).
- ADR 0006 needs a superseding entry naming nila as the host.
- Firmware devices need new MQTT credentials issued and flashed.

## Second migration: nila corrupted, replaced (2026-08-22)

The nila host above (`192.168.1.45` / `100.100.38.32`) was itself
corrupted. It was replaced with a new Raspberry Pi, still called "nila",
at `192.168.1.41` / Tailscale `100.76.190.23` (the addresses used
throughout this doc now). See
`docs/architecture/adr/0011-nila-host-replaced-after-corruption.md` for
full details. Unlike the Radxa migration, this one was **not** a
from-scratch-secrets reset — the owner had the previous host's
`MQTT_API_PASSWORD`/device MQTT credentials and `JWT_SECRET` recorded on
the dev machine (`apps/api/.env`,
`apps/firmware/egg_incubator_v2/secrets.h`) and asked to carry them
over rather than regenerate, so the "generated fresh directly on nila"
line in step 3 above describes the *general* pattern, not what actually
happened this time.

Still open from this migration:
- `coop_monitor_v1` firmware and any ESP32 WiFi provisioning — owner
  handling directly, not tracked here.
- Android app's `API_BASE_URL` still points at `100.100.38.32` (the
  *previous* nila, itself never updated from the Radxa-era address —
  see the unresolved item above). Needs updating to `100.76.190.23`.
