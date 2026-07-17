# Smart Poultry Farm — Agent Directory

This package contains the agent-team specification for the Smart Poultry Farm
Management System. Coordination rules, sequencing, and escalation paths are
defined in `CLAUDE.md` at the repository root — read that first.

**Two client surfaces**: an Android app (field workers, offline-first) and a
web dashboard (admins/farm managers), both consuming one shared API. See
`CLAUDE.md` → "Client surfaces" for the split.

Each agent file below follows a consistent structure: **Role, Owns, Does Not
Own, Reads Before Acting, Produces, Definition of Done, Escalates To,
Skills**. The "Does Not Own" and "Escalates To" sections exist specifically
to prevent overlapping ownership between agents that cover related ground.

## Agents (in typical sequencing order — see `CLAUDE.md` for full detail)

1. [Requirements Engineer](../agents/requirements-engineer.md) — user stories, acceptance criteria, business rules
2. [Poultry Domain Expert](../agents/poultry-domain-expert.md) — incubation, vaccination, candling, flock management facts
3. [Chief Product Architect](../agents/chief-product-architect.md) — product vision, scope, prioritization, coordination
4. [System Architect](../agents/system-architect.md) — bounded contexts, domain model, Clean Architecture/DDD
5. [Database Architect](../agents/database-architect.md) — PostgreSQL schema, migrations, audit/history, reporting data shape
6. [IoT Integration Architect](../agents/iot-integration-architect.md) — MQTT contract, device lifecycle, telemetry, offline sync (device side)
7. [Backend Architect](../agents/backend-architect.md) — Fastify APIs, business logic, auth, background jobs, mobile sync/versioning strategy
8. [UI/UX Architect](../agents/ui-ux-architect.md) — wireframes/flows for both surfaces, accessibility
9. [Android Architect](../agents/android-architect.md) — native Android field app, offline-first, push notifications
10. [Frontend Architect](../agents/frontend-architect.md) — web admin dashboard (Next.js)
11. [QA Engineer](../agents/qa-engineer.md) — test strategy across web, Android, backend, and IoT
12. [Security & DevOps Engineer](../agents/security-devops-engineer.md) — threat modeling, OWASP, CI/CD, Android release/signing, infra
13. [Documentation Engineer](../agents/documentation-engineer.md) — continuous doc maintenance and consistency

## Documentation Index

**Product** (`docs/product/`)
- [PRD](product/PRD.md) — scope, priorities, surface split, success metrics
- [Roadmap](product/roadmap.md) — Phases 0–3 with exit criteria
- [Domain Knowledge](product/domain-knowledge.md) — poultry facts: incubation, candling, vaccination, feed/water, traceability
- [Business Rules](product/business-rules.md) — BR-001…BR-015
- [User Stories](product/user-stories/) — one file per module (16 modules)

**Architecture** (`docs/architecture/`)
- [Domain Model](architecture/domain-model.md) — bounded contexts, entity catalog, traceability chain, canonical enums
- [System Architecture](architecture/system-architecture.md) — topology, monorepo layout, layering, key flows
- [NFRs](architecture/nfr.md) — scale, retention, availability, performance targets
- ADRs: [0001 Supabase hosting](architecture/adr/0001-database-hosting-supabase.md) · [0002 BLE supplementary channel](architecture/adr/0002-ble-supplementary-device-channel.md) · [0003 Tenancy](architecture/adr/0003-tenancy-model.md) · [0004 Mosquitto broker](architecture/adr/0004-mqtt-broker-mosquitto.md) · [0005 Interim web field entry](architecture/adr/0005-interim-web-field-entry.md) · [0006 Radxa always-on host](architecture/adr/0006-radxa-always-on-host.md) · [0007 API via systemd](architecture/adr/0007-api-deployment-systemd.md)

**API** (`docs/api/`)
- [OpenAPI contract](api/openapi.yaml) — grows per Phase 1 increment;
  currently setup/auth/species/farms/incubators/devices + collections/
  batches/candling/hatch + alerts
- [Sync & Conflict Strategy](api/sync-conflict-strategy.md) — BR-010
  server-side rules (clientId idempotency, explicit conflicts);
  android-architect builds the client against this

**IoT** (`docs/iot/`)
- [MQTT Topics](iot/mqtt-topics.md) — real topic structure, captured live
  (not guessed) from the actual firmware and broker
- [Telemetry Contract](iot/telemetry-contract.md) — exact JSON payload
  schema, field-by-field, with a captured real example
- [Device Lifecycle](iot/device-lifecycle.md) — provisioning (manual
  today, BLE not yet implemented in firmware), online/offline detection
  via LWT, de-provisioning
- [Firmware Discovery Guide](iot/firmware-discovery-guide.md) — the
  process used to capture the above; kept for future devices/firmware
  changes

**Code & infra**: `apps/api` (Fastify — includes MQTT ingest
`src/infra/mqtt/`, alert evaluation `src/domain/alerting.ts` +
`src/services/alert.service.ts` (BR-006), and FCM push dispatch
`src/infra/fcm/client.ts` (US-NOT-002)), `apps/web` (Next.js — live
incubator telemetry + an Alerts page with ack), `apps/android` (Kotlin +
Compose — login, live incubator status, offline-first candling/hatch
recording via Room + WorkManager, and FCM push notifications
(`push/EggAppMessagingService.kt`), all built and verified for real
on-emulator against the deployed API, including genuine offline/
reconnect testing and a full MQTT→Alert→push chain; BLE not yet),
`packages/db` (Prisma + Species seed,
[setup steps](../packages/db/README.md)), `packages/shared-types`
(canonical enums), `infra/docker` (Mosquitto) + `infra/systemd`/`infra/deploy` (apps/api) —
both deployed and running on the always-on Radxa host per ADR 0006/0007,
not the dev machine — `.github/workflows/ci.yml`.

**Firmware** (separate repo, `egg-incubator-esp32-rtos`): MQTT publish
added alongside the pre-existing Google Sheets telemetry path (both run
independently) — see that repo's `CHANGELOG.md` entry dated 2026-07-17.

**Not yet written** (owned by the named agent): `docs/database/*`
(database-architect), `docs/android/*`, `docs/web/*` wireframes
(ui-ux-architect), `docs/security/threat-model.md`,
`docs/testing/test-strategy.md`.

## Known Gaps Not Yet Covered By A Dedicated Agent

The following responsibilities are folded into an existing agent's scope
rather than given their own file. Split them out if they grow into full-time
concerns:

- **Reporting/analytics data design** — folded into `database-architect`.
- **Notifications delivery mechanics** — folded into `backend-architect`
  (dispatch) and `android-architect` (on-device handling).
- **DevOps** as distinct from security — kept combined in
  `security-devops-engineer` with an explicit primary/secondary split.
