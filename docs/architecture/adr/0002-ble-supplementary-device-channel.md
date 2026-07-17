# ADR 0002: Bluetooth (BLE) as a supplementary device channel, alongside MQTT

- **Date:** 2026-07-17
- **Author agent:** iot-integration-architect
- **Status:** Accepted

## Context
The incubator's ESP32 firmware already exposes a Bluetooth (BLE) interface,
in addition to its existing WiFi/MQTT telemetry path. Two gaps in the
MQTT-only design motivate using it:

- **Local pairing/setup** — provisioning a new incubator today requires it to
  already be on WiFi; BLE lets the Android app configure WiFi credentials and
  bind the device to a farm/incubator record while standing next to it,
  before it ever touches the network.
- **Offline fallback** — if WiFi/MQTT connectivity drops (common in barn
  environments), the Android app can still read live incubator status
  directly over BLE when in physical range, rather than showing stale data.

## Decision
BLE is added as a **supplementary** device-facing channel. MQTT remains the
primary, authoritative channel for ongoing telemetry, remote monitoring, and
cloud-side alerting — nothing about the existing MQTT contract changes.

- **iot-integration-architect** documents the BLE contract (GATT
  service/characteristic layout, pairing flow, and exactly which fields are
  readable/writable over BLE vs. MQTT-only) as a new deliverable,
  `docs/iot/ble-pairing-protocol.md`, with the same "document what the
  firmware actually does, don't invent" discipline as the MQTT contract.
- **android-architect** implements the BLE client (scan, pair, GATT read/
  write) for device provisioning and offline status/fallback. BLE-sourced
  data captured while offline is queued and synced to the backend through
  the same offline-sync pipeline already used for other field-captured
  records — it is not a separate data path from the backend's point of view.
- **backend-architect**'s sync/conflict rule is unchanged: a BLE-captured
  reading arriving late is just another case of "a record queued while
  offline."
- **security-devops-engineer**'s threat model gains a new item: BLE pairing
  authentication (what stops an unauthorized nearby phone from pairing with
  the incubator) — a physical-proximity trust boundary that doesn't exist in
  the MQTT/WiFi path.
- **qa-engineer**'s IoT and Android test strategies gain BLE-specific
  scenarios: pairing success/failure, BLE-to-MQTT handoff when WiFi returns,
  and BLE-captured records syncing correctly once back online.

## Consequences
- No change to MQTT topic structure, telemetry contract, or backend API
  shape.
- New device-facing contract to maintain (`ble-pairing-protocol.md`),
  owned by iot-integration-architect per the existing "device/protocol
  contract" escalation rule in `CLAUDE.md`.
- Android app takes on a second device-communication path (BLE, in addition
  to REST calls to the backend), scoped strictly to pairing/setup and
  offline fallback — not a general-purpose replacement for the API.
