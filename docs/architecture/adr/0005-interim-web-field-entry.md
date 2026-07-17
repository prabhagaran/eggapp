# ADR 0005: Interim field-data entry on the web dashboard

- **Date:** 2026-07-17
- **Author agent:** chief-product-architect
- **Status:** Accepted (time-boxed — revisit when the Android app ships)

## Context
The PRD's surface split makes the Android app authoritative for field data
entry (candling, hatch outcomes, egg collections), and frontend-architect's
spec explicitly forbids silently duplicating that functionality on the web.
But the Android app is built late in Phase 1, and without *any* entry
surface the backend can't be used end-to-end by the sole operator.

## Decision
The web dashboard ships **interim** entry forms for collections, candling,
and hatch outcomes, calling the same API endpoints the Android app will use
(same BR-002/003/008 enforcement server-side). This is a deliberate,
recorded exception to the surface split — not a precedent.

## Consequences
- When the Android app's offline capture works, these web forms are
  demoted to fallback/desk-correction use (not removed — occasionally
  useful), and the PRD surface table remains authoritative.
- No offline capability is built into the web forms (no service workers,
  no queueing) — offline-first remains exclusively an Android concern.
