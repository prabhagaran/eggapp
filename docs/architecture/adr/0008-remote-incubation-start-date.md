# ADR 0008: App can remotely set the device's incubation start date

- **Date:** 2026-07-19
- **Author agent:** iot-integration-architect (with backend-architect)
- **Status:** Accepted — supersedes the "independent by construction"
  design recorded in `docs/iot/telemetry-contract.md`'s day/hatchEpoch
  section as originally written.

## Context

The original MQTT command contract (`docs/iot/mqtt-topics.md`) scoped
US-INC-003 to the four control-loop setpoint values only. The device's
own incubation-day counter (`gSettings.startEpoch` in firmware, exposed
in telemetry as `day`/`daysLeft`/`hatchEpoch`) was deliberately left
out of that contract — settable only via the physical button UI
(`task_ui.cpp`, `UI_ENV_INCUBATION_DAY`) — and `telemetry-contract.md`
documented this as intentional: the device's clock and the app's
`setAt`-derived schedule were "independent by construction, not just by
choice."

In practice this meant every batch needed its start date entered twice:
once in the app (`POST /batches/:id/set`) and once by hand on the
device's own buttons — the exact double-entry the MQTT command channel
was supposed to eliminate for setpoints. The user wants both entry
points to keep working (the physical buttons stay, e.g. for a device
with no network reach, or a farm worker who's already standing at the
incubator), but wants the app's action to also reach the device when it
can.

## Decision

Extend the existing `eggapp/devices/<id>/cmd` command payload with an
optional `startEpoch` field (Unix seconds):

```json
{"version":5,"startEpoch":1721347200}
```

- Firmware (`task_mqtt.cpp`, `applyStartEpoch()`) applies it through the
  **same code path** as the physical-button flow's final OK
  (`task_ui.cpp:733-795`): restores humidity to the egg-type default,
  resets `lastTurnEpoch`, resumes the turner task if lockdown had
  suspended it, and persists `startEpoch`/`lastTurn`/`setHum` to NVS —
  a remotely-set date survives reboot exactly like a locally-set one.
- Bounds-checked before applying (config.h had no existing bound for
  this, unlike the setpoint MIN/MAX pairs): rejected if more than 1 day
  in the future or ~13 months in the past relative to the device's own
  RTC, when the RTC has valid time. No bound is enforced if the RTC
  isn't yet synced (`rtcEpochValid == false`) — the device can't judge
  "reasonable" without a clock, so the value is trusted as-is rather
  than dropped, matching how the physical button flow has no such check
  either.
- Backend: `POST /batches/:id/set` best-effort pushes `startEpoch` to
  the batch's bound device (if any) via the existing `DeviceConfig`
  push/ack/unconfirmed-timeout pipeline (`deviceConfig.service.ts`,
  reused as-is — the version/ack/2-minute-timeout mechanics are
  identical to a setpoint push, only the payload shape differs). Failure
  to deliver (no device bound, broker unreachable) never fails the
  batch transition itself — an incubator with no MQTT wiring, or a
  broker blip, must not block "eggs are set."
- The device→app direction is unchanged: `day`/`hatchEpoch` in telemetry
  still only mirror onto `EggBatch.deviceDay`/`deviceExpectedHatchAt` as
  a read-only cross-check (`infra/mqtt/ingest.ts`), never written back
  into `setAt`/`expectedHatchAt`. Two-way sync is deliberately not
  symmetric: the app's manual record stays authoritative for reporting;
  the device's counter is now *usually* seeded from it but can still be
  set or overridden locally, and the two are allowed to disagree (the
  existing "device day (mismatch)" UI badge is exactly for this case).

## Consequences

- Both entry points from the requirement now exist: physical buttons
  (unchanged) and the app (new). They can disagree — by design, not by
  bug — if the device is offline when the app pushes, or if someone
  re-sets it locally afterward.
- `telemetry-contract.md`'s day/hatchEpoch section is updated to
  describe the new one-way app→device push instead of claiming full
  independence.
- No new device-side settable bound existed before this ADR
  (`START_EPOCH_MAX_FUTURE_SEC`/`START_EPOCH_MAX_PAST_SEC` in
  `task_mqtt.cpp`) — future firmware changes to the day-counter contract
  should keep these in sync with any equivalent app-side validation, if
  one is ever added.
- Same unconfirmed-config alert path as setpoints now also fires for a
  dropped/undelivered start-date push — no separate alerting build-out
  needed, but also means "setpoint not confirmed" and "start date not
  confirmed" currently look identical to a farm manager reading the
  alert; revisit if that ambiguity turns out to matter in practice.
