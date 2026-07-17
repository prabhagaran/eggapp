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
- ADRs: [0001 Supabase hosting](architecture/adr/0001-database-hosting-supabase.md) · [0002 BLE supplementary channel](architecture/adr/0002-ble-supplementary-device-channel.md) · [0003 Tenancy](architecture/adr/0003-tenancy-model.md)

**Not yet written** (owned by the named agent, Phase 0–1): `docs/iot/*`
(mqtt-topics, telemetry-contract, device-lifecycle, ble-pairing-protocol —
iot-integration-architect, requires firmware discovery), `docs/api/*`
(backend-architect), `docs/database/*` (database-architect),
`docs/android/*`, `docs/web/*` wireframes (ui-ux-architect),
`docs/security/threat-model.md`, `docs/testing/test-strategy.md`.

## Known Gaps Not Yet Covered By A Dedicated Agent

The following responsibilities are folded into an existing agent's scope
rather than given their own file. Split them out if they grow into full-time
concerns:

- **Reporting/analytics data design** — folded into `database-architect`.
- **Notifications delivery mechanics** — folded into `backend-architect`
  (dispatch) and `android-architect` (on-device handling).
- **DevOps** as distinct from security — kept combined in
  `security-devops-engineer` with an explicit primary/secondary split.
