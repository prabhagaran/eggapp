# User Stories — Inventory

**Priority:** P3 · **Surfaces:** Web (management), System (deductions)

## US-INV-001: Track stock items [Web]
As the Owner, I want to track feed (bags/kg), vaccines (doses, lot + expiry), and consumables, per farm.
- Given a vaccine item, when I add stock, then lot number and expiry are required (they feed vaccination records).

## US-INV-002: Auto-deduction [System]
As the system, I deduct stock when feed logs and vaccination records reference an inventory item, so stock stays truthful without separate bookkeeping.
- Given a feed log of N kg linked to an item, when it syncs, then the item's quantity drops by N with a transaction record.

## US-INV-003: Low-stock and expiry alerts [Both]
As the Owner, I want warnings before feed runs out or vaccines expire.
- Given an item crosses its low-stock threshold, or a vaccine is within 30 days of expiry, then a warning notification fires. Rules: BR-014 severity semantics
