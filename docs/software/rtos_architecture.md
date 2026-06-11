# RTOS Architecture

!!! info "As-built — verified against `egg_incubator_v2.ino` (FW 2.0.0)"
    This page describes the shipped task layout. The original design-phase
    architecture (6 generic tasks + FSM modules) is preserved in the
    *Design Archive* section.

## Task table (as created in `setup()`)

| Task | File | Core | Priority | Stack | Cadence / role |
|------|------|------|----------|-------|----------------|
| `RTC` | task_rtc.cpp | 1 | 2 | 3072 | reads DS1307 ~1 s, publishes `gRtcTime` |
| `Buttons` | task_buttons.cpp | 1 | 3 | 2048 | 10 ms poll, 50 ms debounce → `uiEventQueue` |
| `UI` | task_ui.cpp | 1 | 2 | 6144 | event-driven (50 ms queue wait) + idle refresh |
| `DHT` | task_sensors.cpp | 1 | 3 | 3072 | DHT22 humidity + cross-check temperature |
| `DS18B20` | task_sensors.cpp | 1 | 3 | 4096 | primary temperature, POR/validity checks |
| `TempCtrl` | task_control.cpp | 1 | **4** | 4096 | incubator heater/humidifier control (~500 ms) |
| `ClimCtrl` | task_climate_control.cpp | 1 | **4** | 4096 | climate heater/cooler/humidifier control |
| `Turner` | task_incubator.cpp | 1 | 2 | 2048 | 10 s poll of epoch-based turn schedule |
| `Fan` | task_incubator.cpp | 1 | 2 | 2048 | applies PWM fan speed |
| `Pump` | task_incubator.cpp | 1 | 2 | 2048 | humidity-deficit misting with 5 min cooldown |
| `Milestone` | task_incubator.cpp | 1 | 1 | 2048 | incubation-day milestones, lockdown |
| `Cloud` | task_cloud.cpp | **0** | 1 | 12288 | telemetry 60 s, heartbeat 5 min, HTTPS |
| `WifiMgr` | task_wifi_manager.cpp | **0** | 1 | 8192 | Wi-Fi lifecycle, NTP sync (all radio work) |

Control tasks have the **highest priority (4)**; all network work is pinned to
**Core 0** so it can never starve control on Core 1. The Arduino `loop()` is empty.

## Inter-task communication

### Queues (fixed-size structs, no heap)

| Queue | Depth | Payload | Producer → Consumer |
|-------|-------|---------|---------------------|
| `uiEventQueue` | 10 | `UiEvent` | Buttons → UI |
| `errorQueue` | 20 | `ErrorMsg_t` | any (`pushError`) → Cloud |
| `telemetryQueue` | 16 | `TelemetryMsg_t` | Cloud retry/backoff buffer |

### Mutexes (domain-per-mutex)

| Mutex | Protects |
|-------|----------|
| `sensorMutex` | `gSensorData` (temps, humidity, validity flags) |
| `rtcMutex` | `gRtcTime` (DateTime + epoch) |
| `settingsMutex` | `gSettings` (setpoints, profile, schedules) |
| `controlMutex` | `gRelayState` mirror |
| `milestoneMutex` | milestone banner label |

`overTempFault` is shared with ISRs/critical paths via a **spinlock**
(`portENTER_CRITICAL(&faultMux)`), not a mutex. Wi-Fi request flags use
`std::atomic`.

All takes use **bounded timeouts** (30–100 ms). Since BUG-007, the safety-critical
GPIO write in `setRelay()` happens *before* the mutex — a contended mutex can no
longer drop a relay command, only delay the state-mirror update.

### Profile switching (suspend/resume handshake)

1. `switchProfile()` records the new profile in `gSettings` (early-returns if the
   mutex can't be taken — BUG-019).
2. Sends `TASK_CMD_SUSPEND` via `xTaskNotify` to each outgoing-profile task.
3. Tasks block in `xTaskNotifyWait` instead of `vTaskDelay`, so they wake
   immediately even mid-actuation, drive their relay OFF, set their bit in the
   `suspendAckGroup` event group, then `vTaskSuspend(NULL)`.
4. `switchProfile()` waits on the ack bits (3 s safety net — tasks actually ack in
   milliseconds since BUG-008), then resumes the new profile's tasks.

### Task watchdog (TWDT, 10 s — BUG-001)

Subscribed: `TempCtrl`, `ClimCtrl`, `DS18B20`, `DHT`. Each calls
`esp_task_wdt_reset()` at the top of its loop; control tasks deregister before
self-suspending on profile switch and re-add on resume. A hung control task now
panics and reboots rather than leaving the heater frozen ON. Long-running tasks
(turner ≤ 130 s, pump, cloud, UI) are deliberately not subscribed.

## Startup sequence (as-built)

1. Relay pins driven OFF (before anything else)
2. I²C, sensors, RTC (RTC failure no longer halts boot — BUG-009), OLED
3. NVS settings load with range validation (BUG-018)
4. Queues, mutexes, event group creation
5. Task creation (table above)
6. Profile-based suspension of inactive tasks, then TWDT subscription
