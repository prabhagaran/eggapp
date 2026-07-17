# User Stories — Vaccination

**Priority:** P2 · **Surfaces:** Web (templates/compliance), Android (recording)

## US-VAC-001: Vaccination templates [Web]
As the Owner, I want editable per-species/purpose templates (seeded from domain-knowledge §6) defining age, vaccine, disease, route.
- Given the seeded layer/broiler templates, when I edit or add items, then future schedules use my version; the seed is a starting point, not hardcoded truth.

## US-VAC-002: Due-schedule generation and reminders [System, Android push]
As the system, I generate each flock's vaccination schedule from its hatch/acquisition date + template, and remind ahead of time.
- Given a due date approaches, when the lead window (default 24 h, configurable) starts, then a push fires deep-linking to the recording form; overdue items escalate to the web panel.

## US-VAC-003: Record administration [Android, offline]
As a Worker, I want to record a vaccination with all compliance fields (vaccine, lot, expiry, route, dose, count, administrator, reactions), offline in the field.
- Given I save, when it syncs, then the record is immutable; corrections are appended amendments referencing the original. Rules: BR-005, BR-010
- Given required fields are missing, then the form cannot be saved.

## US-VAC-004: Compliance view [Web]
As the Owner, I want per-flock given vs. due vs. overdue at a glance, and full history for traceback.
- Given any flock, when I open compliance, then every schedule item shows administered (with record) / due / overdue status.
