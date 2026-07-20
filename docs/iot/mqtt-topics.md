# MQTT Topics

- **Owner agent:** iot-integration-architect
- **Status:** v1 (2026-07-17) — captured from the real firmware
  (`egg-incubator-esp32-rtos`, `task_mqtt.cpp`) and observed live on the
  broker, not assumed. Supplements the Google Sheets path (ADR 0002);
  MQTT does not replace it.

## Broker

- Eclipse Mosquitto 2.x, running on the always-on Radxa host (ADR 0004),
  not the dev laptop — see ADR 0006.
- Auth required (`allow_anonymous false`); per-purpose username/password.
- No TLS yet (LAN-only deployment) — see `device-lifecycle.md` for the
  exposure caveat.

## Topic structure

Prefix: `eggapp/devices/<DEVICE_ID>` where `DEVICE_ID` is the firmware's
`config.h` identity string (e.g. `INCUBATOR_01`) — **not a MAC address**,
despite the API's `hardwareId` field name (see Reconciliation below).

| Topic | Direction | QoS | Retained | Payload |
|---|---|---|---|---|
| `eggapp/devices/<id>/telemetry` | device → platform | 0 | no | JSON, see `telemetry-contract.md` |
| `eggapp/devices/<id>/status` | device → platform | 1 | **yes** | `"online"` or `"offline"` (plain string, not JSON) |
| `eggapp/devices/<id>/cmd` | platform → device | 1 | no | JSON, see "Commands" below |
| `eggapp/devices/<id>/cmd/ack` | device → platform | 1 | no | JSON, see "Commands" below |

## Client identity & connection

- MQTT client ID = `DEVICE_ID` (one connection per device; a duplicate
  client ID would kick the existing connection — fine, since exactly one
  physical device owns each ID).
- `status` doubles as MQTT Last Will and Testament: on connect, the
  device publishes `"online"` (retained); if it disconnects
  ungracefully, the broker publishes `"offline"` (retained) on its
  behalf automatically — no heartbeat-timeout polling needed for basic
  online/offline detection. See `device-lifecycle.md`.
- Reconnect: firmware retries every `MQTT_RECONNECT_BACKOFF_MS` (5 s)
  while WiFi is up; fully independent of the Google Sheets path.

## Reconciliation note (API field naming)

`backend-architect`'s `Device.hardwareId` field (and the OpenAPI
`hardwareId` property) was originally specified generically as "e.g.
ESP32 MAC" before the real firmware was available. The actual firmware
identifies itself by the `DEVICE_ID` compile-time string, which is what
appears in every MQTT topic and in the telemetry payload's `id` field.
**When registering this device via `POST /v1/farms/:farmId/devices`,
`hardwareId` must be set to the exact `DEVICE_ID` value (e.g.
`"INCUBATOR_01"`), not a MAC address** — the ingest module matches
incoming topics against this field verbatim. Field is not renamed (would
touch already-shipped API surface); this note is the correction of
record.

## Commands (US-INC-003 — setpoint config with sent/received/applied ack)

Scope for this increment: the four EGG-incubator control-loop values
already exposed in telemetry as `setTemp`/`setHum` (plus their
hysteresis, newly added to telemetry alongside this — see
`telemetry-contract.md`), plus the incubation start date (`startEpoch`,
added by ADR 0008), plus (the follow-up increment referenced above)
remote on/off control of fan, turner, humidifier, and pump — now that
telemetry mirrors their current on/off and override state (see
`telemetry-contract.md`), a remote toggle UI can show what it's
changing from.

### `eggapp/devices/<id>/cmd` (platform → device)

```json
{"version":4,"tempSetpoint":37.6,"tempHysteresis":0.3,"humSetpoint":62,"humHysteresis":3}
```

```json
{"version":5,"startEpoch":1721347200}
```

```json
{"version":6,"fanOverride":true,"fanOn":true}
```

```json
{"version":7,"turnerOverride":false}
```

- `version` is a per-device monotonic integer, assigned by the backend
  (`DeviceConfig.version`) — lets the device (and backend) tell commands
  apart and ignore anything older than the last one it acted on.
- All fields are optional and independent; only the fields present are
  changed, and any combination (including all thirteen at once) is
  valid in a single payload.
- `fanOverride`/`turnerOverride`/`humidifierOverride`/`pumpOverride`
  (bool) — enters (`true`) or exits (`false`) remote manual control for
  that one actuator, independent of the other three and independent of
  the existing `ControlMode` (which only ever gated heater/cooler).
  While an actuator's override is `true`, its automatic scheduling logic
  (turner interval, fan PWM curve, pump humidity trigger, humidifier
  hysteresis) is bypassed and the relay/PWM follows `fanOn`/`turnerOn`/
  `humidifierOn`/`pumpOn` directly. Setting override back to `false`
  resumes automatic control on the next task iteration. Existing safety
  gates (sensor-invalid, over-temperature fault latch) still force
  outputs off regardless of override.
- `fanOn`/`turnerOn`/`humidifierOn`/`pumpOn` (bool) — the desired state
  while that actuator's override is active. Sending `fanOn` without
  `fanOverride:true` (in this payload or a prior one) updates the stored
  value but has no visible effect until override is actually on.
  Accepts JSON `true`/`false` or numeric `1`/`0` on the wire.
- Setpoint fields are clamped to the existing `config.h` edit limits
  (`TEMP_SETPOINT_MIN/MAX`, `TEMP_HYST_MIN/MAX`, `HUM_SETPOINT_MIN/MAX`,
  `HUM_HYST_MIN/MAX` — the same bounds the physical button UI already
  enforces in `task_ui.cpp`) rather than rejecting an out-of-range value
  outright, so a slightly-off request still lands somewhere sane.
- `startEpoch` (Unix seconds, ADR 0008) sets the device's own
  incubation-day counter — the same value the physical button UI writes
  via `task_ui.cpp`'s `UI_ENV_INCUBATION_DAY` screen. Applying it
  triggers the same side effects as that screen's final OK: humidity
  restored to the egg-type default, `lastTurnEpoch` reset, the turner
  task resumed if lockdown had suspended it, and the value persisted to
  NVS (`task_mqtt.cpp`'s `applyStartEpoch()`) so it survives a reboot.
  Unlike the setpoint fields, an out-of-range `startEpoch` (more than a
  day in the future, or ~13 months in the past, relative to the
  device's own RTC — only checked once the RTC has valid time) is
  **rejected outright, not clamped**: there's no sane "closest valid
  date" to clamp a bogus timestamp to. The app pushes this automatically
  when a batch is set (`POST /batches/:id/set`) if the incubator has a
  bound device — see ADR 0008 for why this is one-way and best-effort,
  not a hard requirement of that endpoint succeeding.

### `eggapp/devices/<id>/cmd/ack` (device → platform)

```json
{"version":4,"state":"received"}
```
```json
{"version":4,"state":"applied"}
```

Firmware publishes `"received"` immediately on parsing a well-formed
command (before taking `settingsMutex`), then `"applied"` once the
mutex-protected write actually lands. Both typically arrive within
milliseconds of each other and of the `cmd` publish — there's no slow
convergence step (unlike, say, waiting for actual temperature to reach
a new setpoint), but the schema still models the states distinctly per
the original design.

Backend-side: no ack within a retry window (2 minutes) flips the
`DeviceConfig` row to `unconfirmed` and raises a warning alert — the
device may be offline, or the command topic message may have been
dropped (QoS 1, but the backend's own subscribe could still miss it
across a restart window). Matches the given/when in
`docs/product/user-stories/incubators.md` US-INC-003.
