# ADR 0006: Radxa SBC as the always-on deployment host

- **Date:** 2026-07-17
- **Author agent:** system-architect (with security-devops-engineer on
  the operational side)
- **Status:** Accepted

## Context
ADR 0004 chose Mosquitto but didn't pin down *where* it runs. It was
initially set up on the dev laptop for local verification. That's wrong
for actual use: the broker and the API server (which consumes MQTT,
evaluates alerts, and dispatches notifications) both need to be
reachable continuously — including overnight during active incubation,
which `nfr.md` already flags as the availability-critical slice. A
laptop that sleeps, closes, or gets carried around breaks both.

The owner has a Radxa SBC (Debian 12, aarch64) already running full-time
at home, on the same LAN as the incubator.

## Decision
The Radxa is the deployment host for always-on services:
- **Mosquitto broker** — running now (`infra/docker/docker-compose.yml`
  deployed to the Radxa via Docker; Docker Engine + Compose installed
  directly on the board, not the laptop).
- **API server** — deployed to the Radxa (2026-07-18), running and
  verified against real hardware. **Mechanism revised from the original
  plan below: see ADR 0007** — native Node + systemd, not Docker.
- **Web dashboard** — does not need to be always-on (opened on demand);
  may run on the Radxa too for anytime phone/LAN access, or stay
  laptop-only. Not decided here — low-stakes, revisit whenever.

This is exactly the "one small Docker host runs broker + api + web"
target already described in `system-architecture.md` — this ADR just
names the Radxa as that host, rather than leaving it unspecified.

## Consequences
- SSH key-based auth configured from the dev machine to the Radxa
  (`radxa@192.168.1.44`) — no password auth used for ongoing work.
- Broker survives reboots (`restart: unless-stopped` in the compose
  file) and container restarts (Mosquitto persistence volume — retained
  messages, e.g. device online/offline status, survive both).
- Secret handling: MQTT device/service credentials generated with
  `secrets.token_urlsafe` (not defaults), stored in the gitignored
  firmware `secrets.h` and (pending) `apps/api/.env` — never committed.
- security-devops-engineer's threat model gains a concrete asset (the
  Radxa itself: physical security, SSH exposure, whether it's ever
  reachable beyond the LAN) — currently LAN-only, no port forwarding.
