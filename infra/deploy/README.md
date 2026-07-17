# Deploying apps/api to the Radxa

Per ADR 0007 (refining ADR 0006): the API runs as a native Node process under systemd on
the Radxa (192.168.1.44), not Docker — avoids cross-compiling a pnpm
monorepo for ARM64, and a systemd unit gives the same always-on/auto-
restart guarantee with far less moving parts. Docker remains right for
Mosquitto (a self-contained official image, no build step).

## One-time setup (already done on the current Radxa)

1. SSH key auth to `radxa@192.168.1.44` (see `docs/architecture/adr/0006-radxa-always-on-host.md`).
2. `pnpm` installed globally on the Radxa, symlinked into `/usr/local/bin`
   (Node itself lives at `/opt/node22`, not on PATH by default for
   globally-installed packages — see that ADR for the exact symlink fix
   if setting up a new device).
3. Source deployed to `~/eggapp-app/` (apps/api + its two workspace
   dependencies — see `deploy-api.sh`), built once, `apps/api/.env`
   created directly on the device with production secrets (**never**
   copied from a dev machine's `.env` — `MQTT_URL` in particular must be
   `mqtt://localhost:1883`, not the LAN IP, since the broker and API are
   co-located).
4. `infra/systemd/eggapp-api.service` copied to
   `/etc/systemd/system/eggapp-api.service` on the Radxa, then
   `sudo systemctl daemon-reload && sudo systemctl enable --now eggapp-api`.

## Redeploying after a code change

```
bash infra/deploy/deploy-api.sh
```

Packs `apps/api` + `packages/db` + `packages/shared-types`, ships them,
rebuilds in place, restarts the service. You'll be prompted for the
Radxa's sudo password at the restart step (interactive by design — this
script is meant to be run by a person, not fully unattended).

**If the change includes a schema migration**, run it yourself first:
```
pnpm --filter @eggapp/db db:deploy
```
(from the dev machine, against the same Supabase database — migrations
don't need to run on the Radxa itself, just once against the shared DB.)

## Operating it

- Logs: `ssh radxa@192.168.1.44 "sudo journalctl -u eggapp-api -f"`
- Status: `ssh radxa@192.168.1.44 "sudo systemctl status eggapp-api"`
- Survives reboots (`enabled`) and crashes (`Restart=always`, 5s backoff)
  — verified by force-killing the process and confirming systemd
  respawned it within seconds.
