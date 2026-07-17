# User Stories — Egg Management

**Priority:** P1 · **Surfaces:** Android (collection), Web (assignment)

## US-EGG-001: Record an egg collection [Android, offline]
As a Worker, I want to record collected hatching eggs (source flock, count, date, optional avg weight) while in the coop.
- Given no connectivity, when I save, then the record is queued locally with a visible sync state and syncs later without loss. Rules: BR-010

## US-EGG-002: Storage age tracking [Both]
As the Owner, I want to see how long collected eggs have been stored, because hatchability decays past 7 days.
- Given an unassigned collection, when its age exceeds 7 days, then it is visually flagged and a reminder notification is sent. Rules: BR-011

## US-EGG-003: Assign collections to a batch [Web]
As the Owner, I want to pick collections (age and source flock shown) when creating a batch, preserving the flock → collection → batch traceability link.
- Given collections are assigned, when the batch is created, then each collection records how many of its eggs went to that batch.

## US-EGG-004: Discard eggs [Android]
As a Worker, I want to discard unsuitable eggs (cracked, dirty, deformed) with a reason so counts stay honest.
- Given a collection, when I discard N eggs with a reason, then its available count decreases and the discard is recorded.
