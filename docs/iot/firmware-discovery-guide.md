# Firmware Contract Discovery Guide

- **Owner agent:** iot-integration-architect
- **Purpose:** produce `mqtt-topics.md`, `telemetry-contract.md`,
  `device-lifecycle.md`, and `ble-pairing-protocol.md` from the **real**
  incubator's observed behavior — never from assumptions (agent DoD).
  This is the Phase 0 blocker for all Phase 1 device work.

You need: the powered incubator, a phone with **nRF Connect for Mobile**
(free, Android), and the Mosquitto broker from `infra/docker` running on a
computer on the same network (requires Docker Desktop).

## Part A — MQTT (1–2 hours, one full observation cycle)

1. Start the broker (`infra/docker/`, credentials per its README — create a
   temporary device account for the incubator).
2. Point the incubator at your broker (via whatever config mechanism the
   firmware has — AP setup portal, BLE config, or hardcoded; note which!).
3. Subscribe to everything, with QoS/retain flags visible:
   ```
   docker compose exec mosquitto mosquitto_sub -u api-ingest -P <pw> \
     -t '#' -v -F '%I %t  q%q r%r  %p'
   ```
4. Capture through a **full lifecycle**: device boot, normal telemetry
   (≥30 min to establish frequency), a setpoint change if any existing tool
   can trigger one, and a pull-the-plug reconnect (tests offline buffering —
   does buffered data arrive after reconnect, or is it lost?).
5. Record in the tables below; save the raw log to `docs/iot/captures/`.

| Topic | Direction | Payload example | Fields/units | Frequency | QoS | Retained |
|---|---|---|---|---|---|---|
| _e.g. `incubator/<id>/telemetry`_ | dev→plat | `{"t":37.6,...}` | t=°C… | 30 s | 0 | no |

Also answer explicitly: client-id pattern? LWT (last-will) topic/payload?
Command topics the device subscribes to? Ack topic after a command?
Timestamps in payload or assigned at ingest? Buffering behavior on
disconnect (count/duration retained)?

## Part B — BLE GATT survey (~30 min)

1. In nRF Connect: scan → find the incubator. Record advertised name
   pattern, MAC, and advertised service UUIDs.
2. Connect. Record the pairing method it demands (none / Just Works /
   PIN — and if PIN, where the PIN comes from). This feeds the
   security-devops threat-model item from ADR 0002.
3. Enumerate every service → characteristic: UUID, properties
   (read/write/notify), and — by reading while watching the incubator —
   what each value means and its encoding.

| Service UUID | Characteristic UUID | Props | Meaning | Encoding/units |
|---|---|---|---|---|

Also answer: can WiFi credentials be written over BLE (which
characteristic, what format)? Is BLE available continuously or only in a
setup/pairing mode? What triggers pairing mode?

## Part C — write the contracts

With A+B captured, fill the four contract docs (`mqtt-topics.md`,
`telemetry-contract.md`, `device-lifecycle.md`, `ble-pairing-protocol.md`).
DoD (from `agents/iot-integration-architect.md`): every field typed with
unit and frequency, no invented fields; offline/reconnect behavior stated
explicitly; command ack flow fully specified. If a PRD requirement can't be
met by observed firmware behavior (e.g., no command ack exists), that's a
**chief-product-architect escalation**, not something to paper over.
