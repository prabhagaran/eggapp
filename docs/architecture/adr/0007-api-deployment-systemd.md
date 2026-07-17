# ADR 0007: apps/api deployed via systemd, not Docker

- **Date:** 2026-07-18
- **Author agent:** security-devops-engineer (with system-architect)
- **Status:** Accepted — refines ADR 0006, which named the Radxa as host
  but assumed Docker ("same way" as the broker) without testing it.

## Context
ADR 0006 said the API would move to the Radxa "the same way" as Mosquitto
— i.e., Docker. In practice, Dockerizing `apps/api` means cross-building
(or emulating) ARM64 for a pnpm monorepo with two workspace dependencies
(`packages/db`, `packages/shared-types`), which is meaningfully more
moving parts than Mosquitto's single official pre-built image.

The Radxa already has Node 22 installed natively. Building and running
the TypeScript app directly on it — no container — sidesteps the
cross-compilation question entirely, since `pnpm install` / `tsc` /
`prisma generate` all just run on their native architecture.

## Decision
`apps/api` runs as a plain Node process on the Radxa, managed by a
systemd unit (`infra/systemd/eggapp-api.service`, installed at
`/etc/systemd/system/`):
- `Restart=always` + `RestartSec=5` — the same always-on/crash-recovery
  guarantee Docker's `restart: unless-stopped` would have given.
- `WantedBy=multi-user.target` — starts on boot, same as Docker's
  behavior with a `restart` policy.
- Logs via `journalctl -u eggapp-api`, the native systemd equivalent of
  `docker compose logs`.

Docker remains the right tool for Mosquitto (`ADR 0004`) — a single,
well-tested official image with zero build step is exactly Docker's
sweet spot. It just isn't the right tool for *our own* monorepo app at
this stage.

Redeploy process: `infra/deploy/deploy-api.sh` (tar + scp + remote
build + service restart) — see `infra/deploy/README.md`.

## Consequences
- One dependency to track: Node version on the Radxa (22.x) vs. dev
  machine (24.x) — no Node-24-specific APIs are used, so this is a
  non-issue today, but worth remembering if that changes.
- No image registry, no Dockerfile to maintain for this service.
- Verified end-to-end on the actual device: service survives a forced
  `kill -9` (systemd respawned it within the 5s backoff window), is
  reachable over the LAN, and successfully ingested real telemetry from
  the physical incubator through to a real alert evaluation.
- If a second host ever needs to run this (multi-device redundancy,
  etc.), the manual `pnpm install` + build step in
  `infra/deploy/README.md` would be the thing to reconsider —
  containerizing becomes more attractive at that point, not before.
