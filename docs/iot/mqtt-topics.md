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

## Commands (not yet implemented)

No command topic exists yet — MQTT is currently telemetry-only (device →
platform). Config push (US-INC-003, sent/received/applied states) is
Phase 2+ scope; when added, `eggapp/devices/<id>/cmd` will follow the
same client-implements-what-firmware-supports discipline as this file.
