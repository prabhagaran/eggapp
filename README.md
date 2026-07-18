# Smart Poultry Farm Management System (eggAPP)

Personal-deployment poultry farm management: incubation tracking, environmental
monitoring of ESP32-based incubators (MQTT + BLE), candling/hatching records,
flock management, and vaccination scheduling.

**Two client surfaces:**
- **Android app** — field use: candling, feed/water checks, vaccination
  recording, incubator status, device provisioning via BLE. Offline-first.
- **Web dashboard** — oversight: reports, configuration, user/device management.

Both consume one Fastify API backed by Prisma/PostgreSQL (hosted on Supabase).
ESP32 incubator firmware is complete and out of scope; the platform adapts to
its MQTT/BLE contract.

## Orientation

- [CLAUDE.md](CLAUDE.md) — agent coordination charter (read first)
- [docs/README.md](docs/README.md) — documentation index and agent directory
- [docs/product/PRD.md](docs/product/PRD.md) — scope, priorities, surface split
- [docs/architecture/](docs/architecture/) — domain model, system architecture, NFRs, ADRs

## Status

Phase 1 (incubation core: incubators/devices/collections/batches/candling/
hatch/alerts/setpoints), Phase 2 (flocks, vaccination, feed/water), and
Phase 3 (inventory, reports + CSV export, multi-user invites, multi-farm)
are built, deployed to the always-on Radxa host, and verified end-to-end —
see [docs/README.md](docs/README.md) "Code & infra" and each app's README
for what's been verified and how. Reports/Inventory/Team/multi-farm are
web-only by design (admin/oversight surface, per CLAUDE.md's client-surface
split) — Android stays field-worker-focused. See
[docs/product/roadmap.md](docs/product/roadmap.md) for what's next.
