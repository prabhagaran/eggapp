# Profiles

!!! info "As-built — describes FW 2.0.0"
    The design phase envisioned four data-driven profiles (thermostat, incubator,
    cooler, test chamber). The shipped firmware implements **two**, selected at
    runtime from the System menu and persisted to NVS.

## The two profiles

| | `PROFILE_EGG_INCUBATOR` | `PROFILE_CLIMATE_CHAMBER` |
|---|---|---|
| Control task | `TempCtrl` (task_control.cpp) | `ClimCtrl` (task_climate_control.cpp) |
| Active helper tasks | Turner, Fan, Pump, Milestone | — |
| Actuators | heater, humidifier, fan, turner, pump | heater, cooler, humidifier |
| Safety limit | 39.5 °C over-temp latch | 80 °C over-temp latch |
| Modes | Auto / Manual | Fixed Schedule / Cyclic / Ramp |
| Home screen | incubation day, hatch countdown, milestones | phase (HEAT/COOL/IDLE/RAMP n) |
| Menus shown | egg type, incubation day, turner, fan, pump | climate mode, schedule/cyclic/ramp |

Both profiles share the sensor, RTC, button, UI, cloud, and Wi-Fi tasks, the
settings store, and the alarm/fault machinery.

## How switching works (`switchProfile()`)

1. The new profile is recorded in `gSettings` under `settingsMutex`
   (early-return if the mutex can't be taken — the switch never proceeds
   half-recorded, BUG-019).
2. All relays are forced OFF.
3. Outgoing-profile tasks receive `TASK_CMD_SUSPEND` via `xTaskNotify`. They
   wake immediately from any blocking point (`xTaskNotifyWait` replaced all
   long delays — BUG-008), drive their actuator OFF if mid-run, ack via the
   `suspendAckGroup` event group, and self-suspend.
4. After the acks (3 s safety-net timeout; real ack time is milliseconds),
   incoming-profile tasks are resumed and the control task re-subscribes to the
   task watchdog.
5. The selection is persisted to NVS, so the device boots back into the same
   profile.

## Egg-type presets (incubator profile)

| Preset | Total days | Lockdown day | Candle day | Temp | Humidity |
|--------|-----------|--------------|------------|------|----------|
| Chicken | 21 | 18 | 7 | 37.5 °C | 60 % |
| Duck | 28 | 25 | 10 | 37.5 °C | 75 % |
| Quail | 17 | 14 | 7 | 37.5 °C | 60 % |

Selecting a preset applies its defaults (`applyEggTypeDefaults()`); every value
remains individually editable afterwards. At **lockdown** the milestone task
suspends the turner and raises humidity to 70 %; starting a new batch resets
both automatically (BUG-006).

## Climate modes (climate-chamber profile)

- **Fixed schedule** — heat between a start and end hour (RTC-based)
- **Cyclic** — alternate heat/cool periods (30–1440 min each)
- **Ramp** — up to 8 pre-configured temperature steps with per-step duration;
  the UI shows the step table and active step (view-only)

## What a profile does *not* change

Control algorithms, hardware access paths, the safety latch mechanism, and the
UI framework are common code. A profile only determines **which task set runs**
and **which menus/screens are shown** — consistent with the original
"profiles are data, not logic" philosophy.
