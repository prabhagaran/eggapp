# User Stories — Water

**Priority:** P2 · **Surfaces:** Android (logging), Web (review)

## US-WTR-001: Log a water check [Android, offline]
As a Worker, I want to log water given/refilled per flock (litres) during rounds, offline.
- Given I save a log, when it syncs, then it appears on the flock with standard units. Rules: BR-010, BR-012

## US-WTR-002: Consumption anomaly flag [System]
As the system, I flag water anomalies: a sustained drop (illness signal) or a spike alongside falling feed in hot weather (heat stress, domain-knowledge §7).
- Given an anomaly persists across 2 consecutive logs, when detected, then a warning notification fires naming the suspected pattern.
