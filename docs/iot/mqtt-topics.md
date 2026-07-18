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
`telemetry-contract.md`). Turner/fan/pump/mode are **not** covered yet —
their current values aren't surfaced in telemetry, so a remote edit UI
couldn't show what it's changing *from*; a follow-up increment can add
those once telemetry mirrors them.

### `eggapp/devices/<id>/cmd` (platform → device)

```json
{"version":4,"tempSetpoint":37.6,"tempHysteresis":0.3,"humSetpoint":62,"humHysteresis":3}
```

- `version` is a per-device monotonic integer, assigned by the backend
  (`DeviceConfig.version`) — lets the device (and backend) tell commands
  apart and ignore anything older than the last one it acted on.
- All setpoint fields are optional; only the fields present are changed.
  Firmware clamps each to the existing `config.h` edit limits
  (`TEMP_SETPOINT_MIN/MAX`, `TEMP_HYST_MIN/MAX`, `HUM_SETPOINT_MIN/MAX`,
  `HUM_HYST_MIN/MAX` — the same bounds the physical button UI already
  enforces in `task_ui.cpp`) rather than rejecting an out-of-range value
  outright, so a slightly-off request still lands somewhere sane.

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
