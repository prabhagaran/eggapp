# Smart Poultry Farm Management System (eggAPP)

Personal-deployment poultry farm management: incubation tracking, environmental
monitoring of ESP32 devices (MQTT), coop air-quality/resource monitoring,
candling/hatching records, flock management, vaccination scheduling, inventory
and reporting.

**Client surfaces:**
- **Android app** — field use: candling, feed/water checks, vaccination
  recording, incubator status. Offline-first.
- **Web dashboard** — oversight: multi-farm, reports, configuration,
  user/device management.

Both consume one Fastify API backed by Prisma/PostgreSQL (hosted on Supabase).

**Device surfaces:**
- **Firmware** (`apps/firmware/`) — now in-repo (git subtree, history
  preserved). Two profiles: `egg_incubator_v2` (`INCUBATOR_01`, closed-loop
  control) and `coop_monitor_v1` (`COOP_01`, sensor-only, WiFi provisioned from
  a phone via captive portal). The MQTT/telemetry contract in `docs/iot/`
  remains authoritative for the rest of the platform.
- **Hardware** (`hardware/`) — schematics, PCB, BOM and design notes for both
  boards. Nothing fabricated or electrically verified yet.

## Repository layout

```
apps/       api (Fastify) · web (Next.js) · android (Kotlin/Compose) · firmware (ESP32)
packages/   db (Prisma + seed) · shared-types (canonical enums)
hardware/   incubator + coop-monitor board design (Altium: hardware/eggubator/)
infra/      docker (Mosquitto) · systemd + deploy · ci
docs/       product · architecture (+ADRs) · api · iot
agents/     the 13 specialist agent charters
```

## Orientation

- [CLAUDE.md](CLAUDE.md) — agent coordination charter (read first)
- [docs/README.md](docs/README.md) — documentation index and agent directory
- [docs/product/PRD.md](docs/product/PRD.md) — scope, priorities, surface split
- [docs/architecture/](docs/architecture/) — domain model, system architecture, NFRs, ADRs
- [hardware/README.md](hardware/README.md) — board design, pin-map ownership
- [docs/iot/](docs/iot/) — MQTT topics, telemetry contract, device lifecycle

## Status

**Platform.** Phase 1 (incubation core: incubators/devices/collections/
batches/candling/hatch/alerts/setpoints), Phase 2 (flocks, vaccination,
feed/water), and Phase 3 (inventory, reports + CSV export, multi-user invites,
multi-farm) are built, deployed to the always-on Radxa host, and verified end
to end — see [docs/README.md](docs/README.md) "Code & infra" and each app's
README for what's been verified and how. Reports/Inventory/Team/multi-farm are
web-only by design (admin/oversight surface, per CLAUDE.md's client-surface
split) — Android stays field-worker-focused.

**Coop monitoring.** `Coop` is a first-class entity with its own `COOP`
telemetry profile per
[ADR 0009](docs/architecture/adr/0009-coop-monitoring-devices.md); coop
firmware, top-tab navigation and dashboard strips are in place, with a
`SIMULATE_SENSORS` mode flagged end to end for running without real hardware.

**Hardware.** The incubator board is done and **ordered** — 80 components,
routed, DRC clean, 2 layers, ≈ 152 × 101 mm; 5 boards from Lion Circuits,
ordered 2026-08-14. Nothing has arrived, been assembled or been powered, and
every power-budget figure is still a datasheet estimate rather than a
measurement. Coop-monitor board not started.

**Not yet.** BLE provisioning (blocked on firmware) is the remaining P1 Android
gap; the deployment is LAN-only (`192.168.1.44`) with no tunnel yet. See
[docs/product/roadmap.md](docs/product/roadmap.md) for what's next.
