# User Stories — Candling

**Priority:** P1 · **Surfaces:** Android (capture), Web (review)

## US-CAN-001: Candling reminder [Android]
As a Worker, I want a notification on each scheduled candling day (from the batch schedule) so sessions aren't missed.
- Given a batch reaches a scheduled candling day, when the day starts, then I receive a push that deep-links to the candling form. Rules: BR-002, BR-008

## US-CAN-002: Record a candling session [Android, offline]
As a Worker, I want to record counts (fertile, clear, blood ring/early death, unsure) for a session, working offline in the barn.
- Given a batch in `incubating`, when I save counts, then the batch's viable count updates (locally first, synced per BR-010) and the session appears on the timeline.
- Given counts don't sum to the current viable count, then the form shows the discrepancy and requires confirmation. Rules: BR-002, BR-003

## US-CAN-003: Fertility computed at first candling [System]
As the system, I compute fertility % once the day-7 session is saved, per the fixed definition.
- Given the first session is recorded, when it syncs, then fertility % appears on the batch. Rules: BR-015

## US-CAN-004: Candling results in batch review [Web]
As the Owner, I want per-session results and the viable-count progression visible on the batch, so I can see how a batch degraded over time.
