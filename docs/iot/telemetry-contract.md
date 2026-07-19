# Telemetry Contract

- **Owner agent:** iot-integration-architect
- **Status:** v1 (2026-07-17) — exact schema published by `task_mqtt.cpp`,
  captured live from the real device (see field-by-field notes below).

## `eggapp/devices/<id>/telemetry` payload

JSON, built by hand in firmware (no library) — field order below matches
wire order. Example, captured live:

```json
{"id":"INCUBATOR_01","fw":"2.0.0","profile":"EGG","temp":24.1,"hum":89,
 "setTemp":37.5,"setHum":60,"setTempHyst":0.3,"setHumHyst":3,
 "mode":"AUTO","heater":1,"cooler":0,
 "humidifier":0,"fan":1,"pump":0,"turner":0,"day":44,"daysLeft":0,
 "hatchEpoch":1782345600}
```

| Field | Type | Unit | Notes |
|---|---|---|---|
| `id` | string | — | Matches `Device.hardwareId` (see mqtt-topics.md reconciliation note) |
| `fw` | string | — | Firmware version (`config.h` `FW_VERSION`) |
| `profile` | string | — | `"EGG"` or `"CLIMATE"` — active hardware profile |
| `temp` | number \| `null` | °C | DS18B20 reading; `null` when sensor fault (out of −100..100°C) |
| `hum` | number \| `null` | % | DHT22 reading; `null` when out of 0..100% |
| `setTemp` | number | °C | Current temperature setpoint |
| `setHum` | number (int) | % | Current humidity setpoint |
| `setTempHyst` | number | °C | Current temperature hysteresis (added for US-INC-003 — the remote-config UI needs to show the value it's about to change) |
| `setHumHyst` | number | % | Current humidity hysteresis (same reason) |
| `mode` | string | — | `"AUTO"` or `"MANUAL"` |
| `heater`,`cooler`,`humidifier`,`fan`,`pump`,`turner` | 0 \| 1 | — | Relay states at publish time |
| `day` | int | — | Incubation day. **Only present when `profile:"EGG"`.** |
| `daysLeft` | int | — | Days to expected hatch. **EGG only.** |
| `hatchEpoch` | int | unix seconds | Expected hatch time. **EGG only.** |

**Known gap:** when `profile:"CLIMATE"`, the firmware's Google Sheets
path sends a `phase` field (`HEAT`/`COOL`/`IDLE`) that `task_mqtt.cpp`
does not currently mirror — noted here rather than silently worked
around; add if/when Climate Chamber profile is ever in scope for this
app (it currently isn't — see domain-model.md, no such entity).

## Frequency & freshness

- Published every `MQTT_TELEMETRY_INTERVAL_MS` = 60 s while WiFi and the
  broker are reachable. Not retained — a subscriber that connects
  mid-interval waits up to 60 s for the next value.
- No buffering/replay on reconnect: a gap in connectivity is a gap in
  data, not backfilled. Acceptable at personal scale (see NFR); revisit
  if unattended-offline duration becomes a concern.

## Ingest behavior (backend-architect, this repo)

- Match `id` against `Device.hardwareId`; **unmatched IDs are logged and
  dropped**, never auto-create a device record (mirrors BR-007: telemetry
  from an unbound/unknown device is rejected, not silently stored).
- Persist as `TelemetryReading{ tempC: temp, humidityPct: hum, turnerOn:
  turner, source: "mqtt" }`; `null` sensor values stored as `null`, not
  coerced to 0.
- Update `Device.lastSeenAt` on every telemetry message. Also promotes
  `Device.status` to `active` if it wasn't already (telemetry is itself
  proof-of-life — covers the case where the one-off retained `status`
  message arrived before the device was registered, observed in
  practice). This is one-directional: **offline is never inferred from a
  telemetry gap** — that transition comes only from the `status` topic
  (LWT), never from silence; see `device-lifecycle.md`.
- `day`/`hatchEpoch` (EGG profile only) are mirrored onto
  `EggBatch.deviceDay`/`deviceExpectedHatchAt` for whichever batch is
  currently `incubating` on this device's incubator — a read-only
  cross-check the app surfaces if it disagrees with the batch's own
  `setAt`-derived schedule. **Never** written back to `setAt` or
  `expectedHatchAt` themselves: there is no command in this contract for
  the platform to set the device's day counter (only setpoints are
  remotely settable — see "Commands" in `mqtt-topics.md`), so the
  device's own clock and the app's manually-recorded one are
  independent by construction, not just by choice.
