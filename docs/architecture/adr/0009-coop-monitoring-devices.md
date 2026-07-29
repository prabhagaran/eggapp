# ADR 0009: Coop is a first-class entity; coop monitoring is a second device profile

- **Date:** 2026-07-28
- **Author agent:** system-architect (with iot-integration-architect and
  database-architect)
- **Status:** Accepted — supersedes the implicit "a Device is always an
  incubator controller" assumption baked into `Incubator.deviceId` being
  the only device binding in the domain model.

## Context

Until now every `Device` in the system was an egg-incubator controller.
The binding is `Incubator.deviceId` (0..1, BR-007), and that is the only
relation a device has to anything the farm actually manages. The domain
model (`domain-model.md`) has no concept of the building the birds live
in: `Flock` records the birds and their ledger, but not where they are.
The only trace of a coop anywhere in the schema is a free-text example
(`sourceNote: "coop 1"` on `EggCollection`).

Two things forced the issue at once:

1. **Air-quality and resource monitoring is a coop concern, not an
   incubator one.** CO₂, ammonia, light level, feed hopper level and
   water reservoir level describe a house full of birds. An incubator is
   a sealed cabinet of eggs — ammonia and feed level are not merely
   unmeasured there, they are meaningless. A first pass at this feature
   (2026-07-28, same day) added those five channels to the incubator's
   telemetry contract and rendered them on an incubator card. That was
   wrong and this ADR corrects it before any hardware shipped.
2. **The monitoring hardware is a separate controller.** The existing
   `egg_incubator_v2` firmware is an incubator: RTOS relay control,
   turner scheduling, OLED menu, lockdown logic. A coop node shares none
   of that. It is a different physical box in a different building.

So the question is not "which extra fields go on telemetry" but "what
does a coop device belong to?"

## Decision

### 1. `Coop` becomes a first-class entity

```
Farm 1 ─── * Coop
Coop 0..1 ─── Device        (mirrors Incubator's binding, BR-007 style)
Coop 1 ─── * Flock          (a flock is placed in a coop; nullable)
```

`Coop` carries name, optional capacity, and the device binding.
`Flock.coopId` is **nullable** — flocks that predate this change, and
free-range or unhoused birds, must remain valid.

**Telemetry belongs to the building, not the birds.** This is the
central choice and the reason `Flock.deviceId` was rejected as the
cheaper alternative:

- A flock is a cohort with a lifecycle — it is placed, it is culled or
  sold, it ends. The coop outlives it. Environmental history attached to
  a flock would be orphaned every time a flock ends, and the question
  "what were conditions in this house last winter?" would become
  unanswerable across flock boundaries.
- Birds move between houses. Readings do not move with them.
- Two flocks can share one coop (common at personal scale — a layer
  flock and a small breeder group in one shed). One sensor cannot belong
  to two flocks, but both flocks can sit in one coop.

### 2. Coop monitoring is a telemetry **profile**, not a new topic tree

The telemetry contract already carries `profile` (`"EGG"` / `"CLIMATE"`).
Coop nodes publish `profile:"COOP"` on the **same**
`eggapp/devices/<id>/telemetry` topic, and the same `status` / LWT
topics. No second broker path, no second ingest client, no second
device-lifecycle story — a coop node is provisioned, goes online/offline
and is decommissioned by exactly the mechanisms in
`device-lifecycle.md`.

Per profile:

| Field | EGG | COOP |
|---|---|---|
| `temp`, `hum` | ✅ | ✅ |
| `heater`…`turner` + overrides | ✅ | ✖ (a coop node has no relays) |
| `day`, `daysLeft`, `hatchEpoch` | ✅ | ✖ |
| `co2`, `nh3`, `lux`, `feed`, `water` | ✖ | ✅ (each optional, per sensor fitted) |

`TelemetryReading` stays a single device-scoped table with all columns
nullable. It is **not** split per profile: the rows are the same shape
(device + timestamp + whatever that device measured), the volume is
personal-scale (see NFR), and a reading's owner is already resolvable
through the device's binding. Splitting would duplicate the ingest,
retention and freshness logic for no gain.

### 3. Separate firmware sketch, shared contract

`apps/firmware/coop_monitor_v1/` is its own sketch rather than a third
`ProfileType` inside `egg_incubator_v2`. A coop node would otherwise
carry relay drivers, turner/pump tasks, the OLED menu tree and lockdown
logic it can never execute. The contract — not the codebase — is the
thing that stays shared, which is the same boundary ADR 0002 draws for
the BLE channel.

## Consequences

- **A migration adds `Coop` and `Flock.coopId`.** Additive and nullable;
  existing flocks are untouched and simply have no coop.
- **Incubator telemetry loses the five sensor fields** from its contract.
  The `TelemetryReading` columns stay (a COOP device writes them), so no
  column is dropped and no data is lost — the change is which profile is
  allowed to populate them.
- **`Device` now has two possible bindings** (`Incubator` or `Coop`) and
  they are mutually exclusive. Enforced in the service layer at bind
  time, not by a DB constraint: expressing "exactly one of two optional
  inverse relations" in Postgres requires either a check constraint over
  denormalised columns or a trigger, and both are worse than one guard
  in `device.service.ts` at the single point where binding happens.
- **The dashboard's environmental tiles read from a coop device**, not an
  incubator. Incubators keep their own temp/humidity card.
- A farm with no coop configured sees no environmental section at all —
  the correct empty state, rather than tiles reading zero.

## Alternatives rejected

- **`Flock.deviceId`** — cheapest, no new entity. Rejected: ties
  environmental history to a cohort's lifecycle (see above), and cannot
  represent two flocks in one house.
- **`Device.role` enum + free-text location** — defers the modeling
  decision. Rejected: "which coop" stays untyped, so per-coop history and
  reporting are impossible without a later migration that does this work
  anyway.
- **Reusing `Incubator` for coops** — an incubator has capacity in eggs,
  runs batches, has a turner. Overloading it would make every one of
  those fields conditionally meaningless.
