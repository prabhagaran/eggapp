# Reusable Environmental Control Platform

Documentation for an **ESP32 + FreeRTOS** environmental controller that runs as an
**egg incubator** or a **climate chamber** from a single firmware, switchable at
runtime. Firmware **v2.0.0** is implemented, field-tested, and hardened through a
38-finding bug-review cycle.

---

## 🚀 What the device does today

- 🌡️ **Temperature control** — DS18B20 primary sensor, hysteresis control of a heater
  relay (cooler relay in climate-chamber profile), with an over-temperature safety latch
- 💧 **Humidity control** — DHT22 sensor driving a humidifier relay and a timed
  misting pump with cooldown
- 🥚 **Incubation management** — egg-type presets (chicken / duck / quail), incubation-day
  tracking from a battery-backed DS1307 RTC, automatic egg turner, candling and
  **lockdown** milestones (turner stop + humidity raise)
- 🌀 **Fan** — PWM speed control (LEDC)
- 🖥️ **Local UI** — SSD1306 128×64 OLED, three push buttons (UP / DOWN / OK),
  full menu system for every setting
- ☁️ **Wi-Fi + cloud** — user-gated Wi-Fi, NTP time sync, telemetry + error reporting
  every 60 s to a Google Apps Script endpoint over HTTPS, with offline retry queue
- 🔒 **Safety** — task watchdog on the control tasks, latched + NVS-persisted
  over-temp fault, sensor-validity gates, heater-stuck and sensor-plausibility alarms

## 🔁 Two profiles, one firmware

| | Egg Incubator | Climate Chamber |
|---|---|---|
| Setpoint range | 25–50 °C (37.5 °C default) | same, 80 °C safety limit |
| Actuators | heater, humidifier, fan, turner, pump | heater, cooler, humidifier |
| Modes | auto / manual | fixed schedule / cyclic / multi-step ramp |
| Extras | egg-type presets, milestones, lockdown | per-step ramp table (up to 8 steps) |

Profile switching suspends/resumes the relevant FreeRTOS tasks at runtime with an
event-group handshake — no reboot, no reflash.

---

## 📂 Documentation map

| Section | Contents | Status |
|---|---|---|
| **Overview** | Vision, philosophy, architecture | Design-phase, concepts still valid |
| **Hardware** | Pinout (as-built ✅), sensors, actuators | Pinout current; rest design-phase |
| **Software** | RTOS architecture (as-built ✅), code walkthrough (✅), profiles (✅) | Current |
| **User Interface** | UI overview + button mapping | Current (as-built) |
| **Safety & Logging** | Faults, alarms, telemetry | Current (as-built) |
| **Development** | Flow, MVP, roadmap | Roadmap current |
| **Design Archive** | Original FSM / encoder / Nokia-UI design docs | Historical, superseded |

Pages carrying a *"Design-phase document"* or *"Design archive"* banner predate the
firmware; everything else describes the shipped code in `egg_incubator_v2/`.

### Companion documents (repository root)

- **`FIRMWARE_BUG_REVIEW.md`** — full static-review report, 38 findings
  (BUG-001 … BUG-038), fix status for each
- **`FEATURE_ROADMAP.md`** — 30+ candidate features with effort ratings and
  integration notes

---

## 🎯 Current status (2026-06-11)

- ✔ Firmware v2.0.0 implemented — both profiles operational
- ✔ Wi-Fi, NTP, and cloud telemetry live
- ✔ Bug-review cycle complete: BUG-001 … BUG-038 fixed (one hardware item open:
  GPIO12/15 strapping-pin re-pin, BUG-004)
- ✔ UI hardened: single-press menu entry, non-blocking NTP, guarded factory reset
- ➡️ Next: flash repartition (`min_spiffs`) → OTA updates, MQTT/Home Assistant —
  see `FEATURE_ROADMAP.md`

---

## 👨‍💻 Intended audience

Embedded engineers, hardware developers, makers building reliable systems, and
anyone learning **RTOS-based system design** on the ESP32.
