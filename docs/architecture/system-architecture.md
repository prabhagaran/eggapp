# System Architecture

- **Owner agent:** system-architect
- **Status:** v1 (2026-07-17)

## Components & topology (personal deployment)

```
ESP32 incubator ──MQTT──▶ Broker ──▶ apps/api (Fastify)
       │                  (Docker,     │  ├─ REST API  ◀── apps/web (Next.js)
       └────BLE────▶ Android app ──────┘  │             ◀── apps/android (Kotlin)
        (provisioning,   (offline queue,  ├─ MQTT ingest module
         offline reads)   sync via REST)  ├─ background jobs (alerts, heartbeat
                                          │   timeout, reminders, FCM dispatch)
                                          └─ Prisma ──▶ Supabase Postgres (ADR 0001)
                                     FCM ──▶ Android push
```

One small Docker host runs broker + api + web; Supabase hosts Postgres; FCM
handles push. BLE never reaches the server — BLE-captured data enters via
the Android offline-sync pipeline like any field record (ADR 0002).

## Monorepo layout (pnpm workspaces)

```
apps/api        Fastify — routes / services / domain / infra layers
apps/web        Next.js App Router (frontend-architect)
apps/android    Kotlin + Compose (android-architect; Gradle, not pnpm — sibling by convention)
packages/db     Prisma schema + client (database-architect)
packages/shared-types  API contract types (generated from openapi.yaml)
infra/ci        Pipelines (security-devops-engineer)
infra/docker    compose: broker, api, web
```

## Layering & dependency rules

`routes → application services → domain → data (Prisma)`; dependencies point
inward only. Auth/RBAC and farm-scoping (BR-013) are enforced in the
**application-service layer**, not per-route (backend-architect DoD). The
MQTT ingest module is an alternative *inbound adapter* calling the same
services as REST — business rules never live in the adapter.

## Key flows

- **Telemetry:** device → MQTT → ingest → validate against telemetry
  contract → TelemetryReading append + Incubator status cache → alert
  evaluation (BR-006 sustained/stage-aware) → Alert + FCM/web notification.
- **Config change:** Web → API → DeviceConfig (`sent`) → MQTT command →
  device acks per iot contract → `received`/`applied` → UI states
  (US-INC-003); retry/timeout per iot-integration-architect's contract.
- **Offline sync:** Android queue (Room) → batch upload with client UUIDs +
  versions → server applies or returns explicit per-record conflicts
  (BR-010) → client resolves per `docs/api/sync-conflict-strategy.md`.
  Idempotent by client UUID — a retried upload never duplicates.
- **Heartbeat:** revised from the original plan below — online/offline is
  event-driven via MQTT's Last Will (retained `status` topic), not a
  background timeout job; see `docs/iot/device-lifecycle.md`. Telemetry
  still updates `lastSeenAt` as a freshness signal.
- **BLE provisioning:** Android ↔ device per `docs/iot/ble-pairing-protocol.md`;
  app then registers binding via REST (US-DEV-001).

## Cross-cutting decisions

- **Auth:** JWT access (short) + refresh (rotating); Android stores tokens
  in Keystore-backed storage (security-devops review).
- **API versioning:** URL-prefixed (`/v1`); server tolerates N−1 Android
  versions (backend-architect spec) — matters even personally, since the
  phone app won't always update in lockstep.
- **Time:** store UTC; farm timezone drives schedules (candling days,
  quiet hours).
- **Reporting:** SQL views/rollups in Postgres (database-architect owns
  shape) — no separate analytics store at this scale (see NFR).

## Resolved (were "Pending ADRs" here)

- **MQTT broker choice** — Mosquitto (ADR 0004).
- **API/broker hosting target** — self-hosted Radxa SBC, not a PaaS
  (ADR 0006), API via systemd not Docker (ADR 0007).

## Still open

- **Web dashboard hosting** — laptop-only (opened on demand) vs. also
  deployed to the Radxa for anytime phone/LAN access. Not decided; low
  stakes either way (see ADR 0006 consequences).
