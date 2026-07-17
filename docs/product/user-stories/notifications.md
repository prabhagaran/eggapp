# User Stories — Notifications

**Priority:** P1 · **Surfaces:** Web (rules, panel), Android (push)

## US-NOT-001: Configure alert rules [Web]
As the Owner, I want to configure thresholds, sustain windows, severities, and quiet hours (non-critical only) per incubator/scope.
- Given quiet hours, when a **warning** fires inside them, then it is panel-only; **critical** always pushes. Rules: BR-006, BR-014

## US-NOT-002: Push notifications with deep links [Android]
As a user, I want pushes (FCM) for alerts and reminders (candling due, vaccination due, device offline) that open the relevant record when tapped.
- Given any notification, when I tap it, then the app navigates to the specific batch/incubator/flock — never just the home screen.

## US-NOT-003: Alert acknowledgement [Both]
As the Owner, I want to ack alerts, and critical alerts to nag until acked.
- Given an unacked critical alert, when 15 min (configurable) passes, then it re-notifies; acking from either surface stops it everywhere. Rules: BR-014

## US-NOT-004: Notification history [Web]
As the Owner, I want a log of all alerts/notifications with their ack state and timing, so I can audit what the system told me during an incident.
