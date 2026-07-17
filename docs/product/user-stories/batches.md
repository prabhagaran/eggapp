# User Stories — Batches

**Priority:** P1 · **Surfaces:** Web (planning), Both (viewing)

## US-BAT-001: Create an incubation batch [Web]
As the Owner, I want to create a batch by selecting species, egg collections, and an incubator, so the full schedule is planned for me.
- Given selected collections (with storage-age shown), when I create the batch with a set date, then candling days, lockdown day, and expected hatch date are auto-generated from the species reference.
- Given eggs >7 days old, then I see a warning; >14 days is blocked without an override note. Rules: BR-008, BR-011

## US-BAT-002: Batch lifecycle transitions [System]
As the system, I move batches through their lifecycle so field tasks appear at the right time.
- Given the set date arrives, when eggs are confirmed set, then status becomes `incubating`; at day n−3 `lockdown` (with device config push per BR-004); after hatch recording, `completed`. Rules: BR-001, BR-004

## US-BAT-003: Batch timeline [Both]
As a user, I want a chronological view of a batch (set, candlings, lockdown, hatch, notes, env excursions) so I can review what happened.
- Given any batch, when I open its timeline, then all recorded events appear in order with links to their records.

## US-BAT-004: Abort a batch [Web]
As the Owner, I want to abort a failed batch (power failure, contamination) with a reason.
- Given an active batch, when I abort with a reason, then status is `aborted`, scheduled tasks/alerts for it stop, and it remains in history. Rules: BR-001
