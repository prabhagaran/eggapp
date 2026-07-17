# User Stories — Feed

**Priority:** P2 · **Surfaces:** Android (logging), Web (review)

## US-FED-001: Log a feed check [Android, offline]
As a Worker, I want to log feed given per flock (type, quantity kg) during rounds, offline.
- Given I save a log, when it syncs, then it appears on the flock with standard units. Rules: BR-010, BR-012

## US-FED-002: Stage-mismatch warning [System]
As the system, I warn when the logged feed type doesn't match the flock's stage (e.g., starter fed to layers), per domain-knowledge §7.
- Given a mismatch, when the log is saved, then a non-blocking warning is shown and noted on the log.

## US-FED-003: Consumption anomaly flag [System]
As the system, I flag a sustained drop in feed consumption (>20% vs. the flock's 7-day average) as a possible early illness indicator (domain-knowledge §5).
- Given the drop persists across 2 consecutive logs, when detected, then a warning notification fires for that flock. Rules: BR-006 (severity semantics)
