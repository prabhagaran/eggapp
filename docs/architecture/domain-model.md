# Domain Model

- **Owner agent:** system-architect — no other agent redefines an entity here;
  changes go through this agent (see `CLAUDE.md`).
- **Status:** v1 (2026-07-17)

## Bounded contexts

| Context | Entities | Notes |
|---|---|---|
| **Farm & Access** | Farm, User, FarmMembership | Tenancy boundary (ADR 0003) |
| **Incubation** | EggCollection, EggBatch, BatchEggSource, CandlingSession, HatchEvent, Incubator, Species (ref) | The core loop |
| **Device & Telemetry** | Device, TelemetryReading, DeviceEvent, DeviceConfig | Adapts to firmware contract (ADR 0002) |
| **Flock Operations** | Flock, VaccinationTemplate, VaccinationRecord, FeedLog, WaterLog, MortalityRecord | |
| **Alerting** | AlertRule, Alert, Notification | Consumes events from all contexts |
| **Inventory** | InventoryItem, StockTransaction | P3 |
| **Reporting** | (read models only) | No entities of its own — views over the others |

Modules from the brief map 1:1 onto these contexts; "egg management,
candling, hatching, batches, incubators" are all facets of **Incubation**
(one context, not five — they share one consistency boundary: the batch).

## Entity catalog

Aggregate roots carry `farm_id` (scoping, BR-013); nested records
(BatchEggSource, CandlingSession, HatchEvent, telemetry/config/events)
scope through their root's foreign key. Cross-context references
(recordedBy, ackedBy) are plain IDs, not FKs. Offline-creatable records
(marked ⇅) also carry a client-generated UUID + version for sync (BR-010).

### Farm & Access
- **Farm** — name, location, timezone.
- **User** — email, password hash, name.
- **FarmMembership** — user ↔ farm, role ∈ {owner, manager, worker}.

### Incubation
- **Species** (seeded reference, from domain-knowledge §1) — name,
  incubation_days, temp/humidity setpoints + tolerances (incubation and
  lockdown), candling_days[], lockdown_offset. Drives BR-008 scheduling.
- **Incubator** — name, capacity, default species, bound device (0..1),
  current setpoints (denormalized from latest applied DeviceConfig).
- **EggCollection** ⇅ — source flock, date, count, discarded[{count,
  reason}], avg_weight?. Available = count − discarded − assigned.
- **EggBatch** — species, incubator, set_date, status (BR-001 enum),
  schedule {candling_days[], lockdown_date, expected_hatch_date},
  viable_count, metrics {fertility, hatch_of_set, hatch_of_fertile}
  (filled at completion, immutable — BR-015).
- **BatchEggSource** — batch ↔ collection, count taken. Preserves
  flock → collection → batch traceability.
- **CandlingSession** ⇅ — batch, day_no, date, counts {fertile, clear,
  blood_ring, unsure}, discrepancy_note?, recorded_by.
- **HatchEvent** ⇅ — batch (1:1), date, counts {hatched, pipped_dead,
  dead_in_shell, unhatched}, discrepancy_note?, resulting flock ref.

### Device & Telemetry
- **Device** — hardware id (MAC), status ∈ {provisioned, active, offline,
  decommissioned}, last_seen, firmware_version, mqtt credential ref.
- **TelemetryReading** (time-series, append-only) — device, ts, temp,
  humidity, turner_state?, source ∈ {mqtt, ble}. Storage strategy per
  database-architect + NFR.
- **DeviceEvent** — device, ts, type ∈ {online, offline, provisioned,
  decommissioned, config_sent, config_received, config_applied}.
- **DeviceConfig** — device, version (monotonic), payload, state ∈ {sent,
  received, applied, unconfirmed}, timestamps. Powers US-INC-003.

### Flock Operations
- **Flock** — name, species, purpose ∈ {layer, broiler, breeder}, origin
  (HatchEvent ref | acquisition {date, age}), placed_count. Stage and
  current_count are **derived** (age → stage; ledger → count, BR-009).
- **VaccinationTemplate** — species/purpose, items [{age_days_from,
  age_days_to, vaccine, disease, route}]. Seeded from domain-knowledge §6.
- **VaccinationRecord** ⇅ — flock, template item?, date, vaccine, disease,
  lot, expiry, route, dose, count, administered_by, reactions?, next_due?.
  **Immutable; amendments reference the original (BR-005).**
- **FeedLog** ⇅ / **WaterLog** ⇅ — flock, datetime, type/quantity (kg / L),
  inventory item ref?, recorded_by (BR-012).
- **MortalityRecord** ⇅ — flock, date, count, cause ∈ {death, cull, sale},
  notes. The only way counts change (BR-009).

### Alerting
- **AlertRule** — scope (incubator | batch-stage | flock | device), metric,
  bounds {warn, critical}, sustain_minutes, severity, quiet_hours?.
  Stage-aware defaults come from Species (BR-006).
- **Alert** — rule, target, triggered_at, severity, state ∈ {open, acked,
  resolved}, acked_by/at. Critical re-notify per BR-014.
- **Notification** — user, alert?/reminder type, channel ∈ {fcm, web},
  sent_at, read_at, deep_link.

### Inventory (P3)
- **InventoryItem** — kind ∈ {feed, vaccine, consumable}, name, unit,
  quantity, lot?, expiry?, low_stock_threshold.
- **StockTransaction** — item, delta, cause (feed log / vaccination /
  manual), ts.

## Traceability chain (the load-bearing relationship)

```
Flock ──1:n── EggCollection ──n:m── EggBatch ──1:1── HatchEvent ──→ Flock
  │                        (BatchEggSource)                           │
  └── VaccinationRecord / FeedLog / WaterLog / MortalityRecord ───────┘
```

Both directions queryable without ad-hoc joins (database-architect DoD):
diseased flock → hatch → batch → collections → source flock, and forward.

## Lifecycle enums (canonical — clients don't invent states)

- Batch: `planned, setting, incubating, lockdown, hatching, completed,
  closed, aborted` (BR-001)
- Device: `provisioned, active, offline, decommissioned`
- Config: `sent, received, applied, unconfirmed`
- Alert: `open, acked, resolved`
- Sync state (client-side only, android-architect): `queued, syncing,
  synced, conflict`
