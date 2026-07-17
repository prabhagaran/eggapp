# Device Lifecycle

- **Owner agent:** iot-integration-architect
- **Status:** v1 (2026-07-17) — reflects what's actually implemented,
  not an aspirational design. Two provisioning paths exist at different
  maturity levels; both are stated plainly below.

## Provisioning (current state — manual, not BLE)

ADR 0002 specified BLE as the eventual local-pairing path. **The firmware
does not implement BLE yet** — provisioning today is manual:

1. Broker credential: `mosquitto_passwd` on the Radxa host, account named
   `device-<lowercased-id>` (e.g. `device-incubator_01`) — see
   `infra/docker/mosquitto/README.md`.
2. Firmware: the credential + broker host go into the device's
   `secrets.h` (gitignored, per-device, flashed via USB).
3. Platform record: `POST /v1/farms/:farmId/devices` with `hardwareId`
   set to the firmware's exact `DEVICE_ID` string (see mqtt-topics.md).
4. Bind to an incubator: `POST /v1/farms/:farmId/devices/:id/bind`.

BLE provisioning (scan → pair → configure WiFi → confirm, from the
Android app) remains the Phase 1+ target per ADR 0002, once the firmware
gains a BLE service — tracked in the firmware repo's own
`FEATURE_ROADMAP.md` §5.1 area, not yet started.

## Online / offline detection

Driven by the `status` topic (mqtt-topics.md), **not** heartbeat-timeout
polling:

- Device connects → publishes retained `"online"` on `.../status`.
- Device disconnects ungracefully (power loss, WiFi drop, crash) → the
  **broker itself** publishes retained `"offline"` on the device's
  behalf, via MQTT's Last Will mechanism registered at connect time.
  This fires within the broker's keepalive window (`MQTT_KEEPALIVE_SEC`
  = 30 s in firmware), independent of whether the backend is even
  running at that moment — the state is correct in the broker regardless.
- Backend ingest subscribes to `status` and mirrors it onto
  `Device.status` + a `DeviceEvent`, rather than inferring offline from
  telemetry gaps. `Device.lastSeenAt` (updated by telemetry) is a
  freshness indicator, not the offline signal.

## De-provisioning

1. `POST /v1/farms/:farmId/devices/:id/decommission` — unbinds, marks
   `decommissioned` (existing backend behavior, BR-007).
2. Revoke the broker credential:
   `mosquitto_passwd -D <passwd-file> device-<id>` on the Radxa, then
   reload/restart the broker container. **Not yet automated** — the API
   decommission call does not currently reach the broker to revoke
   credentials; this is a manual follow-up step. Flagged here rather
   than silently assumed automatic.

## Firmware buffering on disconnect

Confirmed from `task_mqtt.cpp`: **no buffering**. A telemetry publish
that fails (broker unreachable) is simply skipped; the next attempt is
the next scheduled interval. No queue, no retry-with-backoff for MQTT
specifically (unlike the Google Sheets path, which does queue/retry —
see `task_cloud.cpp`). Acceptable at personal scale per NFR; if this
needs to change, it's a firmware change, not a backend workaround.
