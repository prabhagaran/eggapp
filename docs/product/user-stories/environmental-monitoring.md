# User Stories — Environmental Monitoring

**Priority:** P1 · **Surfaces:** Both (viewing), Web (rules config)

## US-ENV-001: Live readings [Both]
As a user, I want live temperature/humidity (and turner state if reported) per incubator.
- Given an online device, when I view it, then readings are ≤60 s old with a freshness indicator.

## US-ENV-002: History charts [Web]
As the Owner, I want charts over 24 h / 7 d / full-batch ranges so I can verify stability across an incubation.
- Given a completed batch, when I open its environmental report, then min/max/avg and excursions for the full batch window are shown (raw where retained, rollups beyond).

## US-ENV-003: Stage-aware alert thresholds [Web config, Both delivery]
As the Owner, I want thresholds that follow the batch stage (lockdown humidity differs from incubation), so alerts are correct without manual editing mid-batch.
- Given a batch enters `lockdown`, when stage thresholds change, then alerting uses the lockdown bounds automatically.
- Given a sustained excursion past the warning window (default 10 min) or any critical-bound breach, then an alert fires per severity. Rules: BR-006

## US-ENV-004: Reading-gap detection [System]
As the system, I flag sensor silence (no reading for 5 min while device claims online) as a device fault distinct from an environmental breach.
- Given a gap is detected, when it persists, then a device alert (not an environmental alert) is raised. Rules: BR-006, BR-007
