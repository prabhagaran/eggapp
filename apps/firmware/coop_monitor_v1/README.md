# Coop monitor node

Sensor-only ESP32 for the bird house (ADR 0009). Publishes
`profile:"COOP"` telemetry on the shared MQTT topic tree
(`docs/iot/telemetry-contract.md`). No relays, no control loop, no
display — anything resembling incubator behaviour belongs in
`../egg_incubator_v2/`.

## Libraries

Install via the Arduino Library Manager:

- **WiFiManager** (tzapu) — captive-portal provisioning. Already required
  by the incubator sketch.
- **PubSubClient** (Nick O'Leary) — MQTT.
- **DHT sensor library** + **Adafruit Unified Sensor** — only if
  `HAS_TEMP_HUM` is 1.
- **BH1750** (claws) — only if `HAS_LIGHT_SENSOR` is 1.

In simulation mode with every `HAS_*` flag at 0, only WiFiManager and
PubSubClient are needed.

## First-time setup

1. Copy `secrets.h.example` to `secrets.h` and fill in the **MQTT**
   settings. WiFi credentials are *not* in this file — see below.
2. Set `DEVICE_ID` in `config.h` (default `COOP_01`). This string must
   match the `hardwareId` you register in the app **exactly**, including
   case — ingest matches it verbatim and drops unknown ids (BR-007).
3. Flash.

## Connecting it to WiFi from a phone

On first boot there is no stored network, so the node raises its own
access point:

1. On the phone, join the WiFi network **`COOP_SETUP`** (open, no
   password by default).
2. A setup page opens automatically. If it doesn't, browse to
   `http://192.168.4.1`.
3. Tap **Configure WiFi**, pick your network, enter its password, save.
4. The node reboots into your network. The AP disappears — that is the
   success signal.

The portal times out after 3 minutes (`WIFI_PORTAL_TIMEOUT_SEC`). On
timeout the node goes back to retrying its stored network rather than
sitting in AP mode, since the usual cause is a router that was slow
returning after a power cut.

### Changing networks later

**Power the board on, then press and hold the BOOT button for ~2 seconds
within the first 5 seconds.** This forces the portal open even when a
working network is stored. No reflash, no cable.

Note the ordering: press it *after* power-on, not held through it. GPIO0
is a strapping pin — pulled low at reset it puts the ESP32 into serial
download mode and the sketch never runs at all. The serial log prints a
prompt when the window opens.

The portal is never opened automatically on a running device. A node that
dropped into AP mode by itself would stop publishing while looking
identical to one that had died, and nobody is watching its serial output.
Reconnection retries the stored network indefinitely instead.

## Registering it in the app

1. **Coops** page → add a coop.
2. **Devices** page → register with Hardware ID = your `DEVICE_ID`
   (e.g. `COOP_01`). Ignore the "ESP32 MAC" placeholder — see the
   reconciliation note in `docs/iot/mqtt-topics.md`.
3. **Devices** page → *Bind to…* → pick the coop under **Coops**.
4. Create the broker credential on the Radxa
   (`infra/docker/mosquitto/README.md`), using **no `-c`** so existing
   accounts aren't wiped:

   ```
   docker run --rm -v "$PWD:/work" eclipse-mosquitto:2 \
     mosquitto_passwd -b /work/passwd device-coop_01 '<password>'
   docker compose restart mosquitto
   ```

Readings appear within ~60 s.

## Simulation mode

`SIMULATE_SENSORS 1` (the default) publishes plausible invented values for
every channel with no sensor hardware attached, so the pipeline can be
brought up before any sensor exists. Those payloads carry `"sim":1`, land
in `TelemetryReading.simulated`, and the dashboard marks them
**SIMULATED DATA**.

Set it to 0 and reflash when real sensors are fitted. Never strip the flag
to tidy the UI — invented ammonia readings are indistinguishable from real
welfare data once they are rows in a table.

## Before trusting real readings

- **Ultrasonic levels:** measure `FEED_FULL_CM` / `FEED_EMPTY_CM` (and the
  water pair) on the actual rig. The defaults produce confident, wrong
  percentages.
- **Ammonia:** the MQ-137 conversion is a linear placeholder, not an
  Rs/R0 curve, and the sensor needs a long burn-in. Not calibrated.
