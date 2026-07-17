# Offline Sync & Conflict Strategy (server side)

- **Owner agent:** backend-architect (BR-010; android-architect builds the
  client against this)
- **Status:** v1 (2026-07-17) — covers the P1 record types

## Principle

The server is the source of truth. P1's offline-capturable records
(egg collections, candling sessions, hatch events) are **append-only** —
they are created in the field and never edited afterward (candling/hatch
immutability follows from BR-003/BR-005 discipline). That collapses the
conflict problem to exactly two cases:

## 1. Duplicate delivery → idempotent replay

Every offline-created record carries a client-generated UUID (`clientId`).
On create:

- `clientId` unseen → record created → **201**.
- `clientId` already exists for the same target → the **original stored
  record** is returned unchanged → **200**. The client marks its queue item
  synced. A retried upload can never duplicate data, regardless of how many
  times the request is repeated.
- `clientId` exists but belongs to a different batch/farm → **409
  `conflict`** (client bug or UUID collision; surface to the user, don't
  auto-resolve).

The Android queue should treat any 200/201 as success and remove the item.

## 2. Stale-state rejection → explicit conflict response

A queued record can arrive after the world changed (batch aborted while the
phone was offline, counts no longer reconciling). The server never guesses:

| Response | Meaning | Client behavior |
|---|---|---|
| 409 `not_incubating` / `invalid_transition` | Batch state no longer accepts this record (BR-002/BR-001) | Show the record + batch state; user resolves (usually discard with note) |
| 400 `discrepancy_note_required` | Counts don't reconcile with server-side viable count (BR-003) | Prompt user for a discrepancy note, resubmit same `clientId` |
| 409 `too_early` | Hatch date before allowed window (BR-008) | Surface for correction |
| 409 `insufficient_eggs` | Collection over-allocated while offline | Surface for correction |

Resubmission after user correction reuses the **same `clientId`** — the
create is still idempotent.

## Not yet needed

Updated-at/version-based merge resolution applies only to *editable*
resources. No P1 offline-editable resource exists; if Phase 2 adds any
(e.g., editing a feed log), this document gains a version-column protocol
**before** android-architect implements editing. Client-side sync states
(`queued/syncing/synced/conflict`) are defined in `packages/shared-types`.
