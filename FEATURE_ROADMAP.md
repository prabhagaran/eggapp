# Feature Roadmap — Egg Incubator / Climate Chamber (ESP32 + FreeRTOS)

| | |
|---|---|
| **Project** | egg-incubator-esp32-rtos — `egg_incubator_v2/` |
| **Baseline** | FW 2.0.0 + bug-fix series BUG-001 … BUG-038 (2026-06-11) |
| **Purpose** | Candidate new features and functions, grouped by area, with effort and integration notes |

> ⚠️ **Flash budget caveat:** the sketch currently uses **95 % of the 1.25 MB default app partition**.
> Anything marked **[needs partition change]** requires switching the partition scheme
> (e.g. `min_spiffs` or a custom table) or trimming existing code first. OTA in particular
> needs *two* app slots, so it is impossible at the current size without a new partition map.

Legend — Effort: 🟢 small (hours) · 🟡 medium (days) · 🔴 large (week+) · 🛠 needs new hardware

---

## 1. Safety & Reliability

| # | Feature | Why | Integration notes | Effort |
|---|---------|-----|-------------------|--------|
| 1.1 | **Local audible alarm (buzzer)** | Over-temp latch, sensor failure, and heater-stuck warnings are currently silent unless the user looks at the OLED or cloud. A piezo on a spare GPIO beeping on `FAULT`/`HEATER_WARN` errors is the single cheapest hatch-saver. | Drive from the existing `errorQueue` consumer or a tiny `task_alarm`; respect a UI mute setting in `gSettings`. | 🟢 🛠 |
| 1.2 | **Second DS18B20 with voting / failover** | BUG-020 added a DHT22 plausibility *warning*; a second DS18B20 on the same OneWire bus allows actual failover and 2-of-3 voting (with DHT temp) instead of just warning. | `task_ds18b20` already scans the bus — extend to enumerate two addresses; control task picks median/agreeing value. | 🟡 🛠 |
| 1.3 | **Water-level sensor for humidifier reservoir** | Pump running dry is both a hardware risk and a silent humidity failure. | Float switch on a spare GPIO; gate `task_pump` and push `WATER_LOW` error + OLED `!H2O` flag on the home screen. | 🟢 🛠 |
| 1.4 | **Door-open / lid-open detection** | Suppresses spurious heater-stuck and humidity alarms during candling or egg turning by hand; logs openings. | Reed switch input; pause `HEATER_WARN` timer and humidifier while open. | 🟢 🛠 |
| 1.5 | **Power-outage logging & catch-up report** | After an outage the user has no idea how long the eggs were unheated. Persist boot time + last-seen epoch every few minutes; on boot, compute the gap and show "Power was out 2 h 13 m" on the OLED and in telemetry. | RTC already battery-backed; one NVS key written from `task_rtc` (mind flash wear — write at most every 5 min). | 🟢 |
| 1.6 | **Move pump relay off GPIO12** (closes BUG-004) | Strapping-pin conflict can brick boot with some relay boards. | Re-pin in `config.h` + rewire, or document the eFuse alternative (`espefuse.py set_flash_voltage 3.3V`). | 🟢 🛠 |

## 2. Incubation Features

| # | Feature | Why | Integration notes | Effort |
|---|---------|-----|-------------------|--------|
| 2.1 | **More egg-type presets + custom profile** | Only chicken/duck/quail (+1) today. Add goose, turkey, guinea fowl, and one fully user-editable profile (days, lockdown day, temp, hum, turn interval). | Extend `EggType` enum, defaults table in `config.h`, `applyEggTypeDefaults()`, and the Egg Type screen (already scrollable). Custom profile values live in `gSettings`/NVS. | 🟡 |
| 2.2 | **Day-by-day setpoint schedule (staged incubation)** | Real-world incubation lowers temp/raises hum in stages, not just at lockdown. A small per-day table (or 2–3 stages) per egg type automates this. | The milestone task already computes incubation day; let it apply staged setpoints the same way lockdown does today. Reuse the `RampStep_t` concept from the climate profile. | 🟡 |
| 2.3 | **Candling reminders** | Standard practice at day 7/14 (chicken). Show a milestone banner + buzzer chirp; user dismisses with OK. | `checkMilestone()` already returns labels — add `CANDLE` milestones per egg type; banner display path exists (`gMilestoneLabel`). | 🟢 |
| 2.4 | **Daily cooling / misting period for waterfowl** | Duck/goose hatch rates improve with a scheduled daily cool-down + misting. Configurable start time + duration during which heater pauses and (optionally) pump mists. | New fields in `gSettings`; `task_temperature_control` checks the window via RTC; reuse pump for misting with its own duration. | 🟡 |
| 2.5 | **Batch history / hatch log** | Persist per-batch record (start date, egg type, days run, fault count, outage minutes, user-entered eggs set / hatched) to NVS ring or cloud. Enables hatch-rate tracking over time. | Write a record on "new batch" save (the BUG-006 reset point) and on hatch-day; add one cloud telemetry message type. | 🟡 |
| 2.6 | **Turn counter & "last turned" display** | Reassurance that the turner actually works; show count + time since last turn on the home screen or turner menu. | `task_turner` already updates `lastTurnEpoch`; add a persisted counter and surface it in `oled_show_turner_settings`. | 🟢 |

## 3. Control Quality

| # | Feature | Why | Integration notes | Effort |
|---|---------|-----|-------------------|--------|
| 3.1 | **PID (or PI) temperature control with SSR/PWM heater drive** | Bang-bang + hysteresis gives ±hysteresis swing; embryos prefer ±0.1–0.2 °C. Slow-PWM (e.g. 10 s window) into an SSR gives near-flat temperature. | Keep the existing relay path as fallback/manual mode; add `CONTROL_PID` to `ControlMode`. The over-temp latch and sensor-invalid gates stay exactly as-is on top. | 🔴 🛠 (SSR) |
| 3.2 | **Anti-stratification fan logic** | Run the fan briefly after every heater-on edge and during turner runs to even out gradients, instead of a fixed speed only. | Small state additions in `task_fan` keyed off `gRelayState.heaterOn` edges. | 🟢 |
| 3.3 | **Humidity trend-based pump dosing** | Fixed pump duration overshoots in small chambers. Scale dose by (setpoint − current) and learn from the response slope. | Extend `task_pump` decision with last-dose result; clamp to existing min/max duration. | 🟡 |
| 3.4 | **Sensor calibration offsets in UI** | DS18B20/DHT units have per-part offsets; a ±2.0 °C / ±5 %RH user trim (verified against a reference thermometer) materially improves accuracy. | Two floats in `gSettings` + NVS, applied in `task_sensors`; add a "Calibration" item under System. | 🟢 |

## 4. UI / UX

| # | Feature | Why | Integration notes | Effort |
|---|---------|-----|-------------------|--------|
| 4.1 | **Button auto-repeat (hold to scroll/step)** | 0.1 °C steps and year fields need dozens of presses. After ~600 ms hold, repeat every ~150 ms. | `task_buttons` only; ezButton `getState()` + a per-button hold timer. Already listed as advisory in the bug review. | 🟢 |
| 4.2 | **Inactivity timeout → HOME + display dim** | Walk-away safety (home screen shows alarms) and SSD1306 burn-in mitigation. | Track last-event time in `task_ui`; 60 s → `UI_HOME`, 5 min → `display.dim(true)`/blank, any key wakes. | 🟢 |
| 4.3 | **Long-press OK = Back/Cancel everywhere** | With 3 buttons there is no way to abort an edit; OK always saves. | `task_buttons` measures hold (mechanism exists in fault mode); emit `UI_EVT_LONGOK`; each editor maps it to "discard + back". | 🟡 |
| 4.4 | **Temp/hum history sparkline screen** | A 24 h mini-graph on the OLED answers "was it stable overnight?" without the cloud. | Ring buffer (e.g. 144 samples × 2 bytes, 10-min interval) in RAM; one new screen reachable from HOME via UP/DOWN. | 🟡 |
| 4.5 | **New-batch setup wizard** | Chain Egg Type → Start Date → confirmation into one guided flow instead of three separate menus. | Pure `task_ui` state sequence reusing existing screens. | 🟡 |
| 4.6 | **Status LED heartbeat** | Single RGB/bicolor LED: green breathing = OK, yellow = warning, red flash = fault — readable across the room. | Spare GPIO + LEDC; driven from fault flag + error queue. | 🟢 🛠 |

## 5. Connectivity & Remote

| # | Feature | Why | Integration notes | Effort |
|---|---------|-----|-------------------|--------|
| 5.1 | **MQTT + Home Assistant auto-discovery** | Opens the whole HA ecosystem (dashboards, alerts, automations) with one integration; far more flexible than the bespoke Apps Script path. | PubSubClient on Core 0 alongside `task_cloud`; publish the existing telemetry struct; subscribe for setpoint commands (validate exactly like UI edits). **Telemetry publish and setpoint-command subscribe are done** (`task_mqtt.cpp` — eggAPP's `cmd`/`cmd/ack` topics, temp/hum setpoint + hysteresis only); HA auto-discovery messages are not. | 🟡 |
| 5.2 | **Local web dashboard** | Live view + settings from a phone on the LAN, independent of cloud. | ESPAsyncWebServer with a single embedded gzipped page; reuse the mutex-snapshot readers from `task_ui`. **[needs partition change]** | 🔴 |
| 5.3 | **OTA firmware updates** | The device is now field-deployed; reflashing over USB mid-incubation is disruptive. | ArduinoOTA or web OTA. **[needs partition change — two OTA app slots; impossible at 95 % of a single 1.25 MB slot]**. Pair with a boot-validity rollback flag. | 🔴 |
| 5.4 | **Push alerts (Telegram bot)** | Over-temp, sensor-dead, water-low, power-restored push messages with zero recurring cost. | Small HTTPS POST from the existing error-queue drain in `task_cloud`; throttle per source as today. | 🟡 |
| 5.5 | **Timezone / NTP server setting in UI** | `NTP_UTC_OFFSET_SEC` is compile-time; travellers and DST users need a UI setting. | Offset in `gSettings`; System → Time & Date gains a "UTC offset" row; `task_wifi_manager` reads it before `configTime()`. | 🟢 |
| 5.6 | **Telemetry offline buffering & backfill** | WiFi outages currently lose history; queue N samples in RAM/NVS and backfill timestamps on reconnect. | Extend the existing retry queue in `task_cloud`; epoch is already attached to samples. | 🟡 |

## 6. Data & Diagnostics

| # | Feature | Why | Integration notes | Effort |
|---|---------|-----|-------------------|--------|
| 6.1 | **SD-card / LittleFS CSV logging** | Complete local record for post-hatch analysis without any cloud dependency. | SD on VSPI (🛠) or LittleFS (**[needs partition change]**); a low-priority logger task draining a queue, never blocking control. | 🟡 |
| 6.2 | **Diagnostics screen** | Heap free/min, per-task stack high-water marks, WiFi RSSI, NVS write count, uptime, reset reason — invaluable for field debugging. | `uxTaskGetSystemState()` + `esp_reset_reason()`; one read-only screen under System → Device Info. | 🟢 |
| 6.3 | **Egg weight-loss tracking (manual entry)** | Target ~12–14 % egg weight loss by lockdown is the gold-standard humidity feedback. User weighs sample eggs and enters grams; firmware plots % vs target and suggests humidity changes. | Manual entry via UI (or web/MQTT once 5.1/5.2 exist); calculation is trivial, value is in the trend storage. | 🟡 |
| 6.4 | **Self-test mode** | One menu action pulses each relay for 1 s (with heater excluded or temp-gated), reads both sensors, checks RTC and WiFi — pre-flight check before setting eggs. | New System menu item; reuse `setRelay`; require explicit confirm like factory reset. | 🟡 |

## 7. Hardware Expansion (longer term)

| # | Feature | Why | Effort |
|---|---------|-----|--------|
| 7.1 | **I²C I/O expander (PCF8574/MCP23017) for relays** | Frees GPIO12/15 strapping pins (BUG-004) and scales channels for hatcher+setter combos. | 🟡 🛠 |
| 7.2 | **SHT31/SHT40 humidity sensor upgrade** | DHT22 is the least reliable part in condensing 60–75 %RH air; SHT4x is faster, more accurate, and I²C (bus already present). Keep DHT path as a compile-time fallback. | 🟡 🛠 |
| 7.3 | **CO₂ sensor + vent servo** | Late-incubation CO₂ buildup lowers hatch rate in tight cabinets; automated fresh-air vent on a servo/damper. | 🔴 🛠 |
| 7.4 | **Separate hatcher zone support** | Run setter (turning, day 1–18) and hatcher (no turning, high humidity) zones from one controller — second sensor pair + relay bank, profile logic largely exists. | 🔴 🛠 |

---

## Suggested order of attack

1. **Quick wins, software-only (one sitting each):** 4.1 auto-repeat · 4.2 inactivity timeout/dim · 2.3 candling reminders · 2.6 turn counter · 5.5 timezone setting · 6.2 diagnostics screen · 1.5 power-outage report · 3.4 calibration offsets.
2. **Cheap hardware, big safety payoff:** 1.1 buzzer · 1.3 water-level switch · 4.6 status LED · 1.6/7.1 strapping-pin fix.
3. **Core value upgrades:** 2.1 egg profiles · 2.2 staged schedules · 5.1 MQTT/Home Assistant · 5.4 Telegram alerts · 2.5 batch history.
4. **Do once, prerequisite for the rest:** repartition flash (custom table or `min_spiffs`) — unlocks 5.2 web dashboard, 5.3 OTA, 6.1 LittleFS logging. Plan this together with 3.1 PID/SSR as the "v3.0" milestone.

*Generated 2026-06-11. Companion document: `FIRMWARE_BUG_REVIEW.md`.*
