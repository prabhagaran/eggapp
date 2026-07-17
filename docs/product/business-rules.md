# Business Rules

- **Owner agent:** requirements-engineer (domain facts validated by poultry-domain-expert)
- **Status:** Baseline v1 (2026-07-17)

Referenced from user stories as BR-###. Domain constants (days, temps,
schedules) come from `domain-knowledge.md` via seeded species reference data
— rules below reference them, they don't restate numbers.

## Batch lifecycle

- **BR-001** Batch statuses: `planned → setting → incubating → lockdown →
  hatching → completed → closed`, plus `aborted` reachable from any active
  status (reason required). No backward transitions. `setting` is an
  optional intermediate — `planned → incubating` directly is valid for
  same-day setting. `completed` is only reachable by recording a hatch
  outcome.
- **BR-002** Candling sessions can only be recorded for a batch in
  `incubating` status. The system schedules candling days from the species
  reference (e.g., chicken 7/14/18); the day-18 session is part of lockdown
  entry. No candling during `lockdown`.
- **BR-003** Eggs marked clear/blood-ring/dead at candling are removed from
  the batch's viable count. At hatch, `hatched + pipped_dead + dead_in_shell
  + unhatched` must equal the viable count at lockdown; discrepancies are
  recorded explicitly, not silently absorbed.
- **BR-004** Lockdown begins at `incubation_days − 3`: turner off, humidity
  setpoint raised (species reference values). Config change is pushed to the
  device; if telemetry still shows turning after lockdown, raise a warning.
- **BR-008** Batch schedule (candling days, lockdown day, expected hatch
  date) is auto-generated from species at creation. A hatch outcome cannot
  be recorded before `expected_hatch_date − 2 days`.
- **BR-011** Assigning eggs >7 days old to a batch triggers a warning;
  >14 days is blocked unless overridden with a note.
- **BR-015** Hatch metrics use the fixed definitions in domain-knowledge §4,
  computed and stored at batch completion — not recomputed later.

## Records integrity

- **BR-005** Vaccination records are immutable once saved; corrections are
  appended as amendment records referencing the original.
- **BR-009** Flock count ledger must conserve: `current = placed + additions
  − deaths − culls − sales`. Every count change is a dated record with a
  cause; direct edits to `current_count` do not exist.
- **BR-012** Feed/water logs are one record per flock per check, with
  standardized units (kg feed, L water).

## Devices & telemetry

- **BR-006** Environmental alerting is **stage-aware** (lockdown humidity
  bounds differ from incubation bounds) and **sustained-based**: warning
  when outside tolerance for a configurable window (default 10 min);
  critical bounds alert immediately. Single-sample spikes (e.g., lid
  opening) must not alert.
- **BR-007** A device is bound to at most one incubator. Telemetry from an
  unbound/decommissioned device is rejected and logged, not stored against
  any incubator. Rebinding requires an explicit unbind first.

## Sync & access

- **BR-010** Server is the source of truth. Offline-created records carry a
  client-generated UUID and version/updated-at; a stale update receives an
  explicit conflict response the client must handle — never a silent
  overwrite in either direction.
- **BR-013** Every query is scoped to farms the user is a member of.
  Roles: Owner (all), Manager (configuration + records, no user/farm
  admin), Worker (record field data + view). Per ADR 0003.
- **BR-014** Critical alerts require acknowledgement; unacknowledged
  critical alerts re-notify every 15 min (configurable) until acked.
