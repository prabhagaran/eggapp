# User Stories — Hatching

**Priority:** P1 · **Surfaces:** Android (capture), Web (review)

## US-HAT-001: Lockdown checklist [Android]
As a Worker, I want a guided lockdown checklist at day n−3 (final candle, confirm turner off, confirm humidity raised) so lockdown is done right.
- Given a batch reaches lockdown day, when I open its task, then the checklist walks me through the steps and records completion; if telemetry later shows turning, a warning fires. Rules: BR-004

## US-HAT-002: Record hatch outcome [Android, offline]
As a Worker, I want to record hatched, pipped-but-died, dead-in-shell, and unhatched counts once the hatch is over.
- Given a batch in `hatching` (not before expected hatch − 2 days), when I save counts, then they must reconcile against the lockdown viable count, with explicit discrepancy notes if not. Rules: BR-003, BR-008

## US-HAT-003: Hatch metrics computed [System]
As the system, I compute fertility, hatch-of-set, and hatch-of-fertile at batch completion and store them permanently.
- Given the hatch outcome syncs, when the batch completes, then all three metrics are stored per the fixed definitions. Rules: BR-015

## US-HAT-004: Transfer chicks to a flock [Android]
As a Worker, I want hatched chicks to become (or join) a flock in one step, so the traceability chain closes.
- Given a completed hatch, when I create a new flock (or append to an existing brooder flock), then the flock references the hatch event and its placed count. Rules: BR-009
