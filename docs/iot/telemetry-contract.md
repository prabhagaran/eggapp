# Telemetry Contract

- **Owner agent:** iot-integration-architect
- **Status:** v1.1 (2026-07-28) — v1 (2026-07-17) captured live from the
  real device; v1.1 adds the optional air-quality/resource channels
  (`co2`/`nh3`/`lux`/`feed`/`water`), which are **additive and
  unverified against hardware** — no such sensor is fitted to the
  current device, so those fields have not yet been seen on the wire.

## `eggapp/devices/<id>/telemetry` payload

JSON, built by hand in firmware (no library) — field order below matches
wire order. Example, captured live:

```json
{"id":"INCUBATOR_01","fw":"2.0.0","profile":"EGG","temp":24.1,"hum":89,
 "setTemp":37.5,"setHum":60,"setTempHyst":0.3,"setHumHyst":3,
 "mode":"AUTO","heater":1,"cooler":0,
 "humidifier":0,"fan":1,"pump":0,"turner":0,
 "fanOverride":0,"turnerOverride":0,"humidifierOverride":0,"pumpOverride":0,
 "day":44,"daysLeft":0,
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
| `fanOverride`,`turnerOverride`,`humidifierOverride`,`pumpOverride` | 0 \| 1 | — | `1` = that actuator is under remote manual override (see "Commands" in `mqtt-topics.md`) and its `*On` field above reflects the override value, not automatic control-loop output |
| `co2` | number \| `null` | ppm | MH-Z19B NDIR. **COOP profile only** — see below |
| `nh3` | number \| `null` | ppm | MQ-137 ammonia. **COOP only** |
| `lux` | number \| `null` | lux | BH1750 ambient light. **COOP only** |
| `feed` | number \| `null` | % | Feed hopper fill, HC-SR04 ultrasonic distance → percent. **COOP only** |
| `water` | number \| `null` | % | Water reservoir fill, same method. **COOP only** |
| `day` | int | — | Incubation day. **Only present when `profile:"EGG"`.** |
| `daysLeft` | int | — | Days to expected hatch. **EGG only.** |
| `hatchEpoch` | int | unix seconds | Expected hatch time. **EGG only.** |

## `profile:"COOP"` — coop monitoring nodes (v1.1, 2026-07-28)

Per **ADR 0009**, a coop monitoring controller is a second device
profile on the *same* topic tree, not a new protocol. It is a physically
separate box (`apps/firmware/coop_monitor_v1/`) in the bird house, bound
to a `Coop`, not an `Incubator`.

A COOP payload carries `id`, `fw`, `profile`, `temp`, `hum`, plus
whichever of the five sensor channels are fitted:

```json
{"id":"COOP_01","fw":"1.0.0","profile":"COOP","temp":28.7,"hum":63.7,
 "co2":408,"nh3":8.9,"lux":475,"feed":74.5,"water":81.4}
```

It **omits** everything that belongs to an incubator: no
`heater`/`cooler`/`humidifier`/`fan`/`pump`/`turner` or their overrides
(a coop node drives no relays), no setpoint/hysteresis fields (no
control loop), and no `day`/`daysLeft`/`hatchEpoch`.

Conversely an EGG payload never carries the five sensor channels.
Ammonia and feed level are not merely unmeasured inside a sealed
incubator cabinet — they are meaningless there. An earlier draft of this
contract (same day, superseded by ADR 0009) put them on the EGG profile;
that was wrong and no hardware ever shipped with it.

### `sim` — fabricated readings

A coop node built with `SIMULATE_SENSORS=1` (see the sketch's `config.h`)
attaches `"sim":1` to every payload and emits **all** channels with
plausible invented values, with no sensor hardware attached. This exists
so the pipeline (device → broker → API → dashboard) can be brought up
and demonstrated before any sensor is fitted.

- Ingest persists it to `TelemetryReading.simulated`. It is recorded on
  **every** profile, not just COOP — a fabricated payload must never be
  storable as real from any device.
- The dashboard and Coops page render a red **SIMULATED DATA** badge and
  dashed tiles whenever the latest reading carries it.
- Real firmware omits the field entirely, so a genuine payload is
  unchanged and `simulated` defaults to `false`.

The flag is not cosmetic. Invented ammonia and CO₂ values are
indistinguishable from real welfare measurements once they are rows in a
table — the column is what keeps a demo run identifiable months later.
To stop producing simulated data, set `SIMULATE_SENSORS` to 0 and
reflash; never strip the flag to tidy up the UI.

### The five channels are optional fields, not optional values

The distinction matters and the two states are deliberately different:

- **Field absent from the payload** = this device has no such sensor
  fitted. The build-time flags in the coop sketch's `config.h`
  (`HAS_CO2_SENSOR` etc.) gate whether the field is emitted at all.
  Consumers render this as "no sensor", not as a missing reading.
- **Field present but `null`** = the sensor is fitted but its current
  reading failed or fell outside the plausible range (same convention
  `temp`/`hum` already use). Consumers render this as a fault.

Both persist as `null` in `TelemetryReading` — the column cannot
distinguish them, and deliberately doesn't try. "Is this device supposed
to have a CO₂ sensor?" is device-capability metadata, not a property of
one reading; if that question ever needs answering after the fact, it
belongs on `Device`, not on every row of a telemetry table. Until then,
the live UI answers it from the most recent reading.

Payload size: all five fields present adds ~60 bytes, well inside
`MQTT_BUFFER_SIZE`.

**Backward compatibility runs both ways.** Old firmware against new
ingest: fields absent, columns stay null, nothing breaks. New firmware
against old ingest: unknown fields are ignored by the Zod schema, which
is non-strict. So firmware and API can be deployed in either order.

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
- Persist as `TelemetryReading{ tempC: temp, humidityPct: hum, heaterOn,
  coolerOn, humidifierOn, fanOn, pumpOn, turnerOn, source: "mqtt" }`
  (each `*On` mapped from the matching 0|1 field); `null` sensor values
  stored as `null`, not coerced to 0.
- The v1.1 optional channels map `co2 → co2Ppm`, `nh3 → ammoniaPpm`,
  `lux → lightLux`, `feed → feedLevelPct`, `water → waterLevelPct`,
  under the same never-coerce-null rule.
- `fanOverride`/`turnerOverride`/`humidifierOverride`/`pumpOverride` and
  the actuator on/off fields are mirrored onto `Device.currentFanOn` /
  `currentFanOverride` / etc. (same snapshot pattern as `currentTempSetpoint`
  below) so a remote-control UI can show current state before toggling it.
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
  `expectedHatchAt` themselves — that direction stays one-way, since
  `setAt` is the record used for reporting and must stay a manual,
  auditable entry rather than something a device can silently overwrite.
  The *other* direction now exists (ADR 0008, superseding the original
  "independent by construction" design here): `POST /batches/:id/set`
  best-effort pushes `startEpoch` to the device via the `cmd` topic (see
  "Commands" in `mqtt-topics.md`), so `day`/`hatchEpoch` are *usually*
  seeded from the app's schedule rather than fully independent of it.
  They can still drift from it — the push can fail to deliver (no
  device bound, broker unreachable, unconfirmed within the 2-minute
  window), or a farm worker can set/override the date locally via the
  physical button UI afterward — which is exactly the case this
  cross-check mirror exists to surface, not eliminate.
