# Smart Poultry Farm Management System — Agent Coordination Charter

## Purpose
This file defines how the 13 specialist agents in `agents/` work together. It is
read by every agent before it begins work, and it is the arbitration reference
when two agents' outputs conflict.

## Coordination Model

### Sequencing (default order of work)
1. **requirements-engineer** + **poultry-domain-expert** — capture/validate user stories,
   acceptance criteria, business rules, and domain constraints. Output: `docs/product/`.
2. **chief-product-architect** — reviews requirements output, sets scope/priority,
   confirms product direction. Output: `docs/product/PRD.md`.
3. **system-architect** — defines bounded contexts, module boundaries, domain model.
   Output: `docs/architecture/domain-model.md`, `docs/architecture/system-architecture.md`.
4. **database-architect**, **backend-architect**, **iot-integration-architect**,
   **android-architect**, **frontend-architect**, **ui-ux-architect** — proceed in
   parallel, each consuming the domain model and any contracts already
   published by another agent (see "Reads Before Acting" in each agent file).
   API and MQTT contracts must be published before the agents that consume
   them start implementation.

### Client surfaces
This product has **two client surfaces**, not one:
- **Android app** (owned by android-architect) — the primary surface for
  field workers; offline-first, used for candling, feed/water checks,
  vaccination recording, incubator status.
- **Web dashboard** (owned by frontend-architect) — for admins/farm managers;
  multi-farm oversight, reporting, user/device/config management.

Both consume the same API contract from backend-architect. Which workflows
belong to which surface is decided once by chief-product-architect (with
system-architect and ui-ux-architect input) and referenced by both frontend
agents — it is not re-litigated per feature.
5. **qa-engineer** — test strategy runs in parallel from step 1 onward (test
   planning), but test execution against real code follows implementation.
6. **security-devops-engineer** — threat modeling and CI/CD scaffolding start early
   (parallel with step 3); security review of implemented code happens continuously,
   not as a final gate.
7. **documentation-engineer** — maintains docs continuously; not a final-step agent.

### Shared source of truth
- All cross-agent decisions (tenancy model, auth strategy, storage choices, MQTT
  broker choice, etc.) are recorded as dated entries in `docs/architecture/adr/`.
  An agent MUST NOT silently overrule a prior ADR — it proposes a new ADR that
  supersedes it, and flags the conflict to whichever agent owns that domain.
- The domain model (`docs/architecture/domain-model.md`) is the single definition
  of entities (Farm, Incubator, Batch, Egg, Flock, Device, etc.) and their
  relationships. No agent invents a conflicting definition of an entity that
  already appears there — changes go through **system-architect**.

### Escalation / arbitration rules
- **Product-level disagreements** (what to build, priority, scope) escalate to
  **chief-product-architect**.
- **Technical/architectural disagreements** (bounded contexts, module boundaries,
  DDD modeling, which layer owns a rule) escalate to **system-architect**.
- **Data contract disagreements** (schema shape, migration strategy) escalate to
  **database-architect**.
- **Device/protocol contract disagreements** (MQTT topic structure, payload
  schema — constrained by firmware that is out of scope for this repo) escalate
  to **iot-integration-architect**; this contract is authoritative and other
  agents adapt to it, not the reverse.
- If chief-product-architect and system-architect disagree with each other,
  product scope wins for *what* gets built; system-architect retains final say
  on *how* it is technically modeled.
- **Surface ownership disputes** (is this workflow Android or web?) escalate
  to **chief-product-architect**; once decided, it's referenced, not
  redebated.
- **Offline sync/conflict rule disagreements** between android-architect and
  backend-architect escalate to **system-architect**; the server remains the
  source of truth across all clients.

## Non-Functional Requirements
Before database-architect, backend-architect, or iot-integration-architect make
capacity-sensitive decisions (storage engine, retention windows, partitioning),
`docs/architecture/nfr.md` must exist and cover at minimum: expected devices per
farm, expected telemetry frequency/volume, data retention period, expected
concurrent users, and whether this is single-tenant or multi-tenant SaaS.

## Out of Scope for This Repository
- ESP32 firmware implementation (already complete, lives elsewhere). Agents may
  only consume the device-facing contract documented in
  `docs/iot/mqtt-topics.md` / `docs/iot/telemetry-contract.md` — they do not
  redesign firmware behavior.
