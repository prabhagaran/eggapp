# ADR 0011: nila host replaced after corruption (second always-on host swap)

- **Date:** 2026-08-22
- **Author agent:** embedded-engineer (build/flash/deploy work), documenting
  on behalf of system-architect / security-devops-engineer per the ADR
  ownership rule — flagged for their review.
- **Status:** Accepted — supersedes ADR 0010's identification of nila as
  `192.168.1.45` / Tailscale `100.100.38.32`. ADR 0007's decision (native
  Node + systemd for apps/api and apps/web, Docker for mosquitto) is
  unchanged and still applies, just retargeted again.

## Context
The nila host named in ADR 0010 (`192.168.1.45`) became corrupted — a
second always-on-host loss, following the same pattern as the Radxa's
decommissioning in ADR 0006. The owner replaced it with a new Raspberry
Pi (still called "nila", Debian 12/trixie aarch64), reachable on the LAN
at `192.168.1.41`, with none of the previous host's setup (no Docker, no
Node/pnpm, no Tailscale, no SSH key auth) — this was a from-scratch
setup, not a redeploy onto surviving infrastructure.

## Decision
The new Pi (`192.168.1.41`, Tailscale `100.76.190.23`) is the deployment
host for always-on services, same shape as ADR 0010 described:
- **Mosquitto broker** — `infra/docker/docker-compose.yml`. Docker
  installed fresh via `get.docker.com`. Deployed and verified running
  (2026-08-22).
- **API server** — native Node 22 + systemd (ADR 0007's mechanism,
  unchanged). Node 22.23.2 and pnpm 11.22.0 installed fresh. Deployed and
  verified running, confirmed `[mqtt] connected` in logs (2026-08-22).
- **Web dashboard** — native Node + systemd. Deployed and verified
  running, reachable on both LAN (`192.168.1.41:3000`) and Tailscale
  (`100.76.190.23:3000`) (2026-08-22).
- **Tailscale** — installed and joined fresh (previous host's tailnet
  membership was lost with it). New tailnet address is
  `100.76.190.23`, replacing `100.100.38.32` everywhere it's referenced.

Full setup and redeploy steps remain `infra/deploy/README.md` — that
document still names the old `100.100.38.32` address in places and needs
a pass to match, flagged below.

## Consequences
- **Credentials were carried over this time, unlike the Radxa→nila
  migration** — the owner explicitly asked to keep the same MQTT
  credentials (`device-incubator_01`, `api-ingest`) and the same
  `JWT_SECRET`, rather than generating fresh ones. This assumes the old
  nila's secrets were recorded somewhere recoverable (they were, from the
  dev machine's local `apps/api/.env` and `apps/firmware/egg_incubator_v2/secrets.h`)
  rather than lost with the corrupted host itself. `DATABASE_URL`/
  `DIRECT_URL` (same Supabase project) and `firebase-service-account.json`
  (same Firebase project) were carried over as before, per ADR 0010's
  precedent for shared external credentials.
- **Repeat of the ADR 0010 mosquitto permissions issue**: the `passwd`
  file crash-looped the container again (root-owned, `600`, unreadable by
  the non-root `mosquitto` user inside the container) until `chmod 644`'d.
  This is now the second time this has bitten a fresh setup — worth
  promoting from "documented gotcha" to a fixed step in
  `infra/docker/mosquitto/README.md`'s instructions, or better, having
  `docker-compose.yml` set the file's permissions itself rather than
  relying on the operator remembering.
- **`apps/firmware/egg_incubator_v2/secrets.h`** (`MQTT_BROKER_HOST`) and
  **`apps/api/.env`** (`MQTT_URL`, dev-machine copy) both updated to
  `192.168.1.41`. `INCUBATOR_01` firmware rebuilt and reflashed against
  the new broker.
- **Follow-up work, not yet done:**
  - `coop_monitor_v1` firmware still points at the old broker host — owner
    is handling this themselves, per conversation.
  - WiFi provisioning on ESP32 devices is being handled by the owner
    directly, not through this deploy.
  - Android app's `API_BASE_URL` still points at the previous Tailscale
    address (`100.100.38.32`, itself already stale from the Radxa-era
    `100.92.177.99` per ADR 0010's own unresolved follow-up); needs
    updating to `100.76.190.23`. Owned by android-architect.
  - `infra/deploy/README.md` and `infra/deploy/deploy-api.sh` /
    `deploy-web.sh` hardcode `nila@100.100.38.32` — needs updating to
    `100.76.190.23` so future redeploys don't silently target an
    unreachable host. Owned by security-devops-engineer /
    documentation-engineer.
  - `docs/README.md`, if it names the host address anywhere (per ADR
    0010's own unresolved follow-up on this point), needs the same
    update.
- Sudo/security posture unchanged from ADR 0010's note: passwordless
  `ALL:ALL` sudo on the `nila` user, still flagged as a simplification to
  revisit if this host is ever exposed beyond Tailscale.
