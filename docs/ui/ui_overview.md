# UI Overview

!!! info "As-built — describes `task_ui.cpp` / `oled_ui.cpp` (FW 2.0.0)"
    The original Nokia-style / rotary-encoder design is preserved in the
    *Design Archive* section. The shipped UI uses an SSD1306 128×64 OLED and
    three push buttons.

## Hardware

- **SSD1306 128×64 OLED** on I²C (0x3C), Adafruit_SSD1306 driver
- **Three buttons**: UP (GPIO 32), DOWN (GPIO 33), OK (GPIO 25) — see *Button Mapping*

## Architecture

The UI is a single FreeRTOS task (`task_ui`, Core 1, priority 2) running an
**event-driven state machine**:

- The button task posts `UI_EVT_UP / DOWN / OK` events to `uiEventQueue` (depth 10).
- `task_ui` blocks on the queue with a 50 ms timeout. On timeout it runs
  time-driven work (home-screen refresh, live sensor refresh on edit screens,
  non-blocking NTP progress); on an event it dispatches to the current
  `UiState` handler.
- All displayed data is read as **snapshots under their domain mutexes** —
  the UI never touches sensors or actuators directly; settings changes go
  through `gSettings` + `saveSettings()` like any other writer.

### Redraw policy

Screens redraw only when something changed (`lastMenuIdx` dirty-check), plus:

- Home screen: every 3 s (sensor values) and on minute change (clock)
- Temperature / humidity edit screens: live current-value refresh every 1 s
- OLED init failure does not halt the system — the UI task idles and all
  control tasks keep running (BUG-009)

## Screen map

```text
HOME (incubator or climate variant)
└─ OK → MAIN MENU
   ├─ Egg Incubator   (10 items, scrolling window of 4)
   │   ├─ Control Mode → Auto / Manual (manual: heater toggle)
   │   ├─ Set Temperature   (live reading + setpoint, 0.1 °C steps)
   │   ├─ Set Humidity      (1 % steps)
   │   ├─ Hysteresis        (temp / humidity)
   │   ├─ Egg Type          (chicken / duck / quail / custom presets)
   │   ├─ Incubation Day    (start-date editor, D/M/Y fields)
   │   ├─ Turner            (interval, duration, Turn Now)
   │   ├─ Fan               (PWM speed %)
   │   ├─ Pump Duration     (5–120 s)
   │   └─ Back
   ├─ Climate Chamber  (6 items)
   │   ├─ Control Mode / Set Temp / Set Hum / Hysteresis
   │   ├─ Climate Mode → Fixed Schedule / Cyclic / Ramp (view)
   │   └─ Back
   ├─ System           (6 items)
   │   ├─ Switch Profile (Egg Incubator ⇄ Climate Chamber)
   │   ├─ WiFi (connect / disconnect)
   │   ├─ Time & Date (manual RTC edit, WiFi/NTP sync)
   │   ├─ Device Info (ID, FW, IP, uptime)
   │   ├─ Factory Reset (No/Yes confirm, defaults to No)
   │   └─ Back
   └─ Back → HOME
```

## Home screens

**Incubator:** mode (AUTO/MAN), clock, incubation day + date, T current/setpoint,
H current/setpoint, hatch countdown or milestone banner, relay states
(heater/fan/humidifier/turner), `!W` indicator when Wi-Fi is down.

**Climate:** mode, clock, T/H current/setpoint, phase (HEAT/COOL/IDLE or RAMP n),
heater/cooler/humidifier states, Wi-Fi indicator.

## Fault override

When the over-temperature latch is set, the UI unconditionally shows the
**fault screen** (alarm banner, live temperature, "Hold OK 3s to Reset") and
discards queued button events. The reset gesture is handled by the button task,
not the UI.

## Behavioral details (post bug-fix series)

- Every menu entry takes effect on the **first** press — the swallowed-first-event
  patterns were removed (BUG-031, BUG-038)
- OK events are rate-limited to one per 300 ms to absorb contact bounce
- NTP sync runs **non-blocking**: buttons stay live during sync, and presses made
  while syncing are discarded instead of replaying afterwards (BUG-033)
- Factory reset requires highlighting **Yes** explicitly; a stray OK lands on
  **No** (BUG-034)
- Climate schedule/cyclic editors edit local buffers and commit on the final OK,
  so the control task never sees half-edited values (BUG-035)
