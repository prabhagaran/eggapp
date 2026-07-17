# PRD — Smart Poultry Farm Management System

- **Owner agent:** chief-product-architect
- **Status:** v1 approved (2026-07-17)

## Vision & target user

A personal farm-management system for a single **owner-operator** running
ESP32-based incubators, replacing notebooks and guesswork with tracked
incubation batches, live environmental monitoring with real alerts, and a
compliant vaccination/traceability record. Occasional additional users
(family/helper) may record field data later.

**Tenancy:** farm-scoped schema, single-tenant personal deployment, RBAC
retained — see ADR 0003. **Database hosting:** Supabase managed Postgres —
ADR 0001. **Device channels:** MQTT primary + BLE supplementary — ADR 0002.

## Surface split (authoritative — referenced, not re-litigated)

| Workflow | Surface |
|---|---|
| Device provisioning (BLE), incubator status in the barn | **Android** |
| Egg collection, candling, hatch recording, vaccination recording | **Android** (offline-first) |
| Feed/water checks, mortality/cull recording | **Android** (offline-first) |
| Batch creation/planning, setpoint & alert-rule configuration | **Web** |
| Multi-farm oversight, dashboards, charts, reports/exports | **Web** |
| User/role management, device fleet management, inventory | **Web** |
| Live incubator status, alert receipt/ack | **Both** (push+ack on Android, panel on Web) |

Rule of thumb: **data captured standing next to birds/eggs → Android;
planning, configuration, and analysis → Web.** Web may display field data
read-only; Android does not host admin configuration.

## Module scope and priority

| Module | Priority | Notes |
|---|---|---|
| Devices | **P1** | BLE provisioning, health, heartbeat/offline detection |
| Incubators | **P1** | Status, setpoints, config ack states |
| Batches | **P1** | Lifecycle + auto schedule (BR-001/BR-008) |
| Egg management | **P1** | Collection, storage age, assignment to batch |
| Candling | **P1** | Scheduled sessions, counts, viable tracking |
| Hatching | **P1** | Lockdown checklist, outcomes, metrics, → flock |
| Environmental monitoring | **P1** | Live + history + stage-aware alerts |
| Notifications | **P1** | Rules, FCM push, ack, history |
| Users / auth | **P1 (minimal)** | Owner login, JWT; invites/roles P3 |
| Farms | **P1 (bootstrap)** | First-run wizard; multi-farm UI P3 |
| Flocks | **P2** | From hatch events; stages; count ledger |
| Vaccination | **P2** | Templates, due schedule, immutable records |
| Feed | **P2** | Logs, stage mismatch warning, anomaly flag |
| Water | **P2** | Logs, anomaly flag (heat stress) |
| Reports | **P2 core / P3 full** | Hatch rates + env history first; CSV export |
| Inventory | **P3** | Stock, auto-deduct, expiry alerts |

Every module from the project brief is scheduled above; none deferred without
a stated priority. Out of scope entirely: billing, marketplace/sales,
firmware changes (external), coop-level sensors (until hardware exists).

## Success metrics (personal deployment)

1. Every batch tracked set → hatch with fertility/hatch rates computed
   automatically (zero manual calculation).
2. Environmental breach → push notification on phone in **< 2 minutes**.
3. **Zero** field records lost to offline conditions (sync loss = 0).
4. Every due vaccination surfaced ≥ 24 h ahead (configurable lead).
5. New incubator provisioned via BLE in < 10 minutes without touching a config file.

## Assumptions & risks

- Firmware capabilities (BLE GATT surface, MQTT topics, offline buffering)
  must be **documented from the real device** before iot contract work —
  the single biggest unknown (iot-integration-architect owns discovery).
- Single small server (or free-tier hosting) + Supabase free tier is
  sufficient at personal scale — validated by NFR volumes.
- FCM requires a Firebase project even for personal use — acceptable.
