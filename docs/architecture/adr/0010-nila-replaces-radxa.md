# ADR 0010: nila (Raspberry Pi) replaces the Radxa as the always-on host

- **Date:** 2026-08-15
- **Author agent:** system-architect (with security-devops-engineer on
  the operational side)
- **Status:** Accepted — supersedes ADR 0006's naming of the Radxa
  (`radxa@192.168.1.44` / Tailscale `100.92.177.99`) as the deployment
  host. ADR 0007's decision (native Node + systemd, not Docker, for
  apps/api and apps/web) is unchanged and still applies, just retargeted.

## Context
The Radxa went unreachable (LAN and Tailscale both timed out) and the
owner confirmed it's decommissioned — not a transient outage. The owner
has a Raspberry Pi ("nila", Debian 12 aarch64, Tailscale `100.100.38.32`)
already set up with Node 22, pnpm, Docker, and SSH key auth, to take over
the same role.

This also closes a gap ADR 0006 explicitly left open: the Radxa was
LAN-only (`192.168.1.44`), with "Tailscale or similar" named as
not-yet-done follow-up work for reachability beyond the home network.
nila is deployed Tailscale-first — `apps/web/.env.production` bakes in
the Tailscale address (`100.100.38.32`), not a LAN IP, so the dashboard
is reachable from any device with Tailscale connected, matching how the
Android app already reaches its backend.

## Decision
nila is the deployment host for always-on services, same shape as ADR
0006 described for the Radxa:
- **Mosquitto broker** — `infra/docker/docker-compose.yml`, Docker
  Engine + Compose already installed on the Pi.
- **API server** — native Node + systemd (ADR 0007's mechanism,
  unchanged), deployed and verified running (2026-08-15).
- **Web dashboard** — native Node + systemd, deployed and verified
  running (2026-08-15), now Tailscale-reachable rather than LAN-only.

Full setup and redeploy steps: `infra/deploy/README.md`.

## Consequences
- SSH key-based auth configured from the dev machine to nila
  (`nila@100.100.38.32`) — no password auth used for ongoing work.
- **Secrets could not be migrated** — the Radxa was unreachable before
  its `apps/api/.env` or mosquitto `passwd` file could be recovered.
  nila runs with freshly generated `JWT_SECRET` and `MQTT_API_PASSWORD`.
  `DATABASE_URL`/`DIRECT_URL` (same Supabase project) and
  `firebase-service-account.json` (same Firebase project) were carried
  over from the dev machine, since those are shared external credentials
  rather than per-host secrets.
- **Follow-up work, not yet done:**
  - Any device MQTT credentials issued against the old broker are
    invalid; devices need new credentials issued and flashed against
    nila's `passwd` file (currently only `api-ingest` is provisioned).
    Owned by iot-integration-architect per the MQTT contract rule.
  - Android app's `API_BASE_URL` still points at the old Radxa Tailscale
    address (`100.92.177.99`); needs updating to `100.100.38.32`
    (`apps/android/README.md` / `build.gradle.kts`). Owned by
    android-architect.
  - `docs/README.md` still describes the Radxa as the deployment host
    and Tailscale as "not started" — needs updating by
    documentation-engineer.
- Sudo posture differs from the Radxa: nila's `nila` user has
  passwordless `ALL:ALL` sudo rather than the Radxa's narrow
  `systemctl restart/status <service>`-only rule — a deliberate
  simplification for initial setup, flagged here for
  security-devops-engineer to revisit if nila is ever exposed beyond
  Tailscale.
- One operational fix worth recording: Mosquitto crash-looped (exit
  code 13, no error logged) until the `passwd` file was `chmod 644`'d —
  the container runs mosquitto as a non-root user and can't read a
  root-owned `600` file even mounted read-only.
