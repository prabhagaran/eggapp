# Roadmap

!!! info "Updated 2026-06-11 to reflect actual progress"
    The detailed, prioritized feature list lives in **`FEATURE_ROADMAP.md`** at
    the repository root. This page tracks the phase history.

## Phase history

### Phase 0 — Documentation & architecture ✔ (completed)

Vision, philosophy, pinout, FSM design, UI style. The original design docs are
preserved under *Design Archive*; implementation deviated where noted (buttons
instead of encoder, task-based control instead of formal FSM modules, no
water-level hardware).

### Phase 1 — Thermostat MVP ✔ (absorbed)

The MVP was absorbed into the incubator build rather than shipped standalone:
FreeRTOS task structure, sensor validation, relay control, hysteresis logic,
OLED UI.

### Phase 2 — Egg incubator ✔ (FW 2.0.0)

Egg-type presets, incubation-day tracking (DS1307 RTC), automatic turner,
milestones + lockdown, misting pump, PWM fan.

### Phase 3 — Climate chamber profile ✔

Second runtime profile with cooler support, fixed-schedule / cyclic / ramp
modes, and live profile switching (suspend/resume handshake).

### Phase 4 — Connectivity ✔

User-gated Wi-Fi (WiFiManager), NTP sync, HTTPS telemetry + error reporting to
Google Apps Script with offline retry.

### Phase 5 — Hardening ✔ (2026-06)

Full static review + fix cycle: **38 findings (BUG-001 … BUG-038) fixed**, from
watchdog coverage and relay-polarity safety to UI single-press fixes — see
`FIRMWARE_BUG_REVIEW.md`. One hardware item remains open (GPIO12/15 strapping
pins, BUG-004).

## Next (Phase 6 candidates)

In suggested order — full details, effort ratings, and integration notes in
`FEATURE_ROADMAP.md`:

1. **Quick wins:** button auto-repeat, inactivity timeout + display dim,
   candling reminders, diagnostics screen, timezone setting
2. **Cheap hardware safety:** buzzer alarm, water-level switch, status LED,
   strapping-pin re-pin
3. **Core upgrades:** more egg presets + custom profile, staged setpoint
   schedules, MQTT / Home Assistant, Telegram alerts
4. **v3.0 milestone:** flash repartition (`min_spiffs` — already configured in
   `sketch.yaml`) → **OTA updates**, local web dashboard, LittleFS logging,
   PID + SSR heater drive
