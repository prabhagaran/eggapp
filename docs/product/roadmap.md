# Roadmap

- **Owner agent:** chief-product-architect
- **Status:** v1 (2026-07-17)

## Phase 0 — Foundations
Monorepo scaffold (pnpm workspaces: `apps/api`, `apps/web`, `apps/android`,
`packages/db`, `packages/shared-types`, `infra/`), Supabase project + Prisma
baseline, CI skeleton, MQTT broker selection ADR + local broker via Docker,
**firmware contract discovery** (document real MQTT topics + BLE GATT —
blocks Phase 1 device work).
**Exit:** `docs/iot/*` contracts exist from observed firmware; empty apps
build and CI runs.

## Phase 1 — Incubation MVP (P1 modules)
Devices (BLE provisioning, heartbeat), incubators (status, setpoints/ack),
batches + egg collections + candling + hatching (full lifecycle, BR-001–011),
environmental monitoring (live, history, stage-aware alerts), notifications
(FCM push + ack), minimal auth (Owner login) + first-run farm wizard.
**Exit:** one real incubation batch run end-to-end on the real incubator:
provision → set eggs → candle (offline in barn) → alert fires on a real
excursion → hatch recorded → metrics computed. Success metrics 1–3, 5 hold.

## Phase 2 — Flock operations ✅ (2026-07-19)
Flocks (from hatch, stage derivation, count ledger), vaccination (templates,
due notifications, immutable records), feed/water logs + anomaly flags,
core reports (hatch performance, environmental history per batch).
**Exit:** hatched chicks tracked as a flock with vaccination compliance
visible; success metric 4 holds. The two core-reports stories (US-RPT-001
hatch performance, US-RPT-002 environmental history) shipped alongside
Phase 3 rather than at initial Phase 2 close — folded into the Phase 3
reporting effort instead of a second pass.

## Phase 3 — Oversight polish ✅ (2026-07-19)
Inventory (stock, auto-deduct, expiry), full reports + CSV export
(hatch performance, environmental history, vaccination compliance,
mortality trends benchmarked against domain-knowledge §5 norms),
multi-user invites/roles, multi-farm UI (create + switch).
**Exit:** all P3 stories closed. Multi-user invite is deliberately simple
for this scale (no email infra — an existing user is added immediately,
a new user gets a one-time temporary password to share out-of-band)
rather than building token-based invite emails for a 1-2-user personal
deployment.
