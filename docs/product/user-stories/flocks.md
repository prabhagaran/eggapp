# User Stories — Flocks

**Priority:** P2 · **Surfaces:** Android (field records), Both (viewing)

## US-FLK-001: Flock creation [Android/Web]
As a user, I want flocks created from a hatch event (US-HAT-004) or manually for acquired birds (provenance noted), so every flock has a known origin.
- Given a manual flock, when I create it, then species, purpose (layer/broiler/breeder), count, and acquisition age/date are required. Rules: BR-009

## US-FLK-002: Stage derivation [System]
As the system, I derive flock stage (brooding/grower/pre-lay/layer, or broiler track) from age so it is never stale.
- Given a flock crosses a stage boundary, when the day arrives, then stage updates and a notification suggests the matching feed change (domain-knowledge §5/§7).

## US-FLK-003: Mortality / cull / sale recording [Android, offline]
As a Worker, I want to record deaths, culls, and sales with a cause, keeping the count ledger conserved.
- Given a record, when it syncs, then `current = placed + additions − deaths − culls − sales` still holds; direct count edits don't exist. Rules: BR-009, BR-010

## US-FLK-004: Flock detail [Both]
As a user, I want one screen per flock: current count, age, stage, vaccination status (due/overdue), recent feed/water, mortality trend, and its origin hatch/batch link (traceability).
