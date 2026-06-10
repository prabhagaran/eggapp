# Firmware Bug Review Report — Egg Incubator / Climate Chamber (ESP32 + FreeRTOS)

| | |
|---|---|
| **Project** | egg-incubator-esp32-rtos — `egg_incubator_v2/` |
| **Firmware version** | 2.0.0 (`config.h`) |
| **Review date** | 2026-06-10 |
| **Review type** | Static analysis — no code modified |
| **Scope** | All `.ino` / `.cpp` / `.h` sources (≈ 5 950 LOC): task logic, RTOS usage, sensor handling, relay/heater/fan/pump control, NVS persistence, Wi-Fi/cloud, watchdog, GPIO safety |

---

## 1. Executive Summary

The firmware is well above hobby average: relay writes are centralized, shared state is mutex-protected, an over-temperature latch exists in both profiles, sensor-invalid gates shut the heater off, profile switching uses a notification/event-group handshake, and secrets are kept out of git. Many earlier review findings (M-7, M-8, F-01) were clearly fixed.

However, the review found **2 Critical**, **7 High**, **12 Medium** and **9 Low** issues. The most important themes:

1. ~~**The task watchdog protects nothing** — it is initialized but no task is subscribed and idle-task monitoring is masked off. A hung control task leaves the heater frozen in its last state with no recovery.~~ **✅ Fixed (BUG-001)**
2. ~~**Fan output polarity is self-contradictory** for the declared active-LOW relay board — `setFanSpeed(0)` drives the pin LOW, which is `RELAY_ON` on that hardware; `allRelaysOff()` may therefore *energize* the fan channel.~~ **✅ Fixed (BUG-002)**
3. ~~**Humidity actuation never checks `hum_valid`** and operates on a fabricated 50 % default — a dead DHT22 can run the humidifier/pump logic on frozen data for the rest of the hatch.~~ **✅ Fixed (BUG-003)**
4. ~~**RTC epoch validity (`rtcEpochValid`) is computed but never enforced**, so all epoch-based logic (turner, cyclic phase, ramp, milestones) silently misbehaves after RTC battery loss (unsigned underflow).~~ **✅ Fixed (BUG-005)**
5. **GPIO12 (pump relay) is a strapping pin** — a relay board pull-up on it can prevent the ESP32 from booting at all.

### Severity Summary

| Severity | Count | IDs |
|----------|-------|-----|
| Critical | 2 | BUG-001, BUG-002 |
| High | 7 | BUG-003 … BUG-009 |
| Medium | 12 | BUG-010 … BUG-021 |
| Low | 9 | BUG-022 … BUG-030 |

---

## 2. Critical Findings

### BUG-001 — Task watchdog is initialized but monitors nothing ✅ FIXED
* **Severity:** Critical → **Fixed** (2026-06-10)
* **Location:** [egg_incubator_v2.ino:394-401](egg_incubator_v2/egg_incubator_v2.ino#L394-L401) (`setup()`); CHANGELOG confirms `esp_task_wdt_add(NULL)` / `esp_task_wdt_reset()` were removed from all tasks.
* **Root cause:** `esp_task_wdt_init(&wdt_cfg)` is called with `idle_core_mask = 0` (no idle tasks watched) and **no task ever calls `esp_task_wdt_add()`**. The TWDT therefore has zero subscribed tasks. Additionally, on Arduino-ESP32 core 3.x the TWDT is already initialized by the framework, so `esp_task_wdt_init()` returns `ESP_ERR_INVALID_STATE` (return value is not checked); `esp_task_wdt_reconfigure()` is the correct call.
* **Impact:** The single most important fail-safe in a heater-controlling device is absent. If `task_temperature_control` deadlocks, hard-faults into a blocked state, or stalls on a wedged I²C/OneWire bus while the heater relay is ON, the heater stays ON indefinitely. Eggs cook; in the climate-chamber profile (80 °C limit) this is a fire-adjacent hazard. The code *looks* protected ("panics on expire" comment) but is not — worse than having no WDT, because it creates false confidence.
* **Fix applied:**
  1. `setup()` now calls `esp_task_wdt_reconfigure()` when `esp_task_wdt_init()` returns `ESP_ERR_INVALID_STATE`, with return-value checking.
  2. Four safety-critical tasks are subscribed: `task_temperature_control` and `task_climate_control` are subscribed from `setup()` via `esp_task_wdt_add(handle)` **after** the profile-based `vTaskSuspend` calls (so the inactive control task is never subscribed while frozen); `task_ds18b20` and `task_sensor` (DHT) self-subscribe in their preambles since they are never suspended.
  3. Each subscribed task calls `esp_task_wdt_reset()` at the top of every loop iteration. On profile switch, the outgoing control task calls `esp_task_wdt_delete(NULL)` before `vTaskSuspend(NULL)` and `esp_task_wdt_add(NULL)` immediately after (resume path).
  4. WDT timeout is 10 s — covers the DHT22 worst-case cycle (≈3.5 s) with 2.8× margin. Turner (up to 130 s), pump (30 s), milestone, cloud, Wi-Fi and UI tasks are intentionally not subscribed.

### BUG-002 — Fan PWM polarity contradicts the active-LOW relay convention; “fan off” can energize the output ✅ FIXED
* **Severity:** Critical → **Fixed** (2026-06-10)
* **Location:** [task_incubator.cpp:161-202](egg_incubator_v2/task_incubator.cpp#L161-L202) (`setFanSpeed`), [globals.cpp:78-86](egg_incubator_v2/globals.cpp#L78-L86) (`allRelaysOff`), [config.h:37-43](egg_incubator_v2/config.h#L37-L43)
* **Root cause:** `RELAY_FAN` (GPIO13) is declared in the “RELAY OUTPUT PINS / Active-LOW relay board” block (`RELAY_ON = LOW`). But `setFanSpeed()` drove it with **non-inverted** LEDC PWM: `percent = 0 → duty 0 → pin constantly LOW`, and `percent = 100 → duty 255 → pin constantly HIGH`. Under active-LOW semantics that is exactly inverted: `setFanSpeed(0)` (used by `allRelaysOff()`, sensor-fault and over-temp paths) held the pin LOW = **relay energized**, while 100 % speed de-energized it. Meanwhile `setup()` wrote `digitalWrite(RELAY_FAN, RELAY_OFF /*HIGH*/)`, which under the PWM convention meant “full speed”. The two conventions cannot both be right.
* **Impact:** Depending on the real hardware:
  * Fan on the relay board → every “safe shutdown” (`allRelaysOff()`, over-temp latch, factory reset) turns the fan channel ON, and 25 kHz PWM applied to a relay/optocoupler input produces undefined relay behavior (chatter, partial drive, coil heating). Mechanical relays cannot be PWM-speed-controlled at all.
  * Fan on a MOSFET/driver (active-HIGH) → the config comment and the boot-time `digitalWrite(RELAY_OFF)` are wrong, and intermediate duty values behave correctly only by accident.
* **Fix applied:**
  1. Duty formula inverted in `setFanSpeed()`: `duty = (100 - percent) * 255 / 100`. `percent=100` now drives `duty=0` → GPIO LOW → active-LOW relay ON → fan runs. `percent=0` drives `duty=255` → GPIO HIGH → relay OFF → fan stopped. `allRelaysOff()` calling `setFanSpeed(0)` now correctly de-energizes the relay.
  2. `ledc_channel_config` initial duty changed from `0` to `255` so the relay starts in the safe OFF state at LEDC init, before `task_fan` applies the configured speed.

---

## 3. High Findings

### BUG-003 — Humidifier/pump logic acts on invalid and fabricated humidity data ✅ FIXED
* **Severity:** High → **Fixed** (2026-06-10)
* **Location:** [task_sensors.cpp:81,103,123-127](egg_incubator_v2/task_sensors.cpp#L81-L127); consumers: [task_control.cpp:131-144](egg_incubator_v2/task_control.cpp#L131-L144), [task_climate_control.cpp:154-165](egg_incubator_v2/task_climate_control.cpp#L154-L165)
* **Root cause:** Three compounding issues: (1) `lastValidHum` is initialized to a fabricated `50.0f` and is written into `gSensorData.humidity_dht` even when the read failed (“stale-hold”), (2) both control tasks read `humidity_dht` but **never check `hum_valid`**, and (3) there is no stale-age limit — the held value is served forever.
* **Impact:** If the DHT22 dies (common failure mode in 60-75 %RH condensing incubator air), the humidifier hysteresis keeps switching on a frozen value indefinitely. Example: last valid reading 56 %, setpoint 70 % at lockdown → humidifier latched ON continuously for days → saturated chamber, drowned/late-drowned embryos, water spillage near mains wiring. Conversely a frozen high reading suppresses humidification entirely. The boot-time fabricated 50 % can also switch the humidifier before a single real reading exists. Note the pump task *does* check `hum_valid` ([task_incubator.cpp:234](egg_incubator_v2/task_incubator.cpp#L234)) — the inconsistency shows the gap is unintentional.
* **Fix applied:**
  1. `task_sensors.cpp` — `lastValidHum` seed removed (`0.0f`, never written to shared state until a real read succeeds). Added `everHadValid` flag: on bad reads, `humidity_dht` is only updated with the stale value if a genuine reading has been obtained at least once; otherwise `hum_valid` is set `false` and `humidity_dht` is left untouched. On a good read, `hum_valid=true` is written immediately inside the valid branch.
  2. `task_control.cpp` — reads `hum_valid` from the sensor mutex block; humidifier section wrapped in `if (!humValid) { setRelay(RELAY_HUMIDIFIER, false); } else { … }`.
  3. `task_climate_control.cpp` — same gate applied to its humidifier section.

### BUG-004 — Pump relay on GPIO12 (strapping pin); turner on GPIO15 (strapping pin)
* **Severity:** High
* **Location:** [config.h:38-39](egg_incubator_v2/config.h#L38-L39)
* **Root cause:** GPIO12 is the ESP32 `MTDI` strapping pin that selects flash voltage at reset. Typical active-LOW relay boards pull their inputs up to VCC through the opto-LED; an external pull-up on GPIO12 at reset sets VDD_SDIO to 1.8 V on 3.3 V-flash modules → the chip fails to boot (brownout/boot loop). GPIO15 (`MTDO`) is also a strapping pin (silences boot ROM log; affects timing on some boards).
* **Impact:** Device may fail to boot at all once the relay board is connected, or boot intermittently depending on relay board input circuit — a “works on bench, dead in field” failure. After any watchdog/brownout reset mid-incubation, a non-booting controller means no heat at all.
* **Recommended fix:** Move the pump relay to a safe GPIO (e.g. 16, 17, 19, 23). If the PCB is already committed, burn the flash-voltage eFuse (`espefuse.py set_flash_voltage 3.3V`) to make GPIO12 strapping irrelevant, and verify GPIO15 boot behavior with the actual relay board attached.

### BUG-005 — `rtcEpochValid` is computed but never enforced; epoch math underflows after RTC failure ✅ FIXED
* **Severity:** High → **Fixed** (2026-06-10)
* **Location:** Flag set in [task_rtc.cpp:24-32](egg_incubator_v2/task_rtc.cpp#L24-L32); never read anywhere. Consumers at risk: [task_incubator.cpp:72](egg_incubator_v2/task_incubator.cpp#L72) (turner), [climate_logic.cpp:57](egg_incubator_v2/climate_logic.cpp#L57) (cyclic), [climate_logic.cpp:103](egg_incubator_v2/climate_logic.cpp#L103) (ramp), [incubator_logic.cpp:101](egg_incubator_v2/incubator_logic.cpp#L101) (milestones)
* **Root cause:** When the DS1307 loses battery backup it resets to year 2000 (epoch ≈ 9.47×10⁸). `task_rtc` correctly detects this and sets `rtcEpochValid = false` — but no control logic checks the flag. All elapsed-time computations use unsigned arithmetic: `(nowEpoch - lastTurn)` with `lastTurn` from 2026 and `nowEpoch` from 2000 underflows to ≈ 4.2×10⁹ s.
* **Impact:** Turner fires immediately and then corrupts `lastTurnEpoch` (persisted to NVS) with a year-2000 value; cyclic heat/cool phase and ramp-step advancement jump arbitrarily; incubation day / milestone calculations go wrong. The same hazard occurs transiently whenever the user sets the clock backwards (manual edit or NTP).
* **Fix applied:** `rtcEpochValid` is now checked at every epoch-dependent entry point before any subtraction is attempted:
  - `task_turner` (`task_incubator.cpp`): after reading `nowEpoch`, `if (!rtcEpochValid)` skips the entire turn decision and delays 10 s — no underflow, no spurious turn, no NVS corruption.
  - `task_milestone` (`task_incubator.cpp`): guard extended to `nowEpoch == 0 || !rtcEpochValid` — milestone and lockdown logic is skipped entirely until the epoch is sane.
  - `calcIncubationDay` (`incubator_logic.cpp`): returns `0` (not started) when epoch is invalid.
  - `checkMilestone` (`incubator_logic.cpp`): returns `false` when epoch is invalid.
  - `cyclicInHeatPhase` (`climate_logic.cpp`): returns `true` (heat phase) when epoch is invalid — the temperature setpoint still bounds the heater, avoiding runaway.
  - `getRampTargetTemp` (`climate_logic.cpp`): returns the current `tempSetpoint` when epoch is invalid — ramp step index is frozen, no premature advancement.

### BUG-006 — Lockdown state is never reset: a second batch gets no turning and no lockdown ✅ FIXED
* **Severity:** High → **Fixed** (2026-06-10)
* **Location:** [task_incubator.cpp:284, 329-352](egg_incubator_v2/task_incubator.cpp#L284-L352) (`task_milestone`), start-date save at [task_ui.cpp:754-770](egg_incubator_v2/task_ui.cpp#L754-L770)
* **Root cause:** On lockdown day `task_milestone` sets the function-local `lockdownApplied = true` and **suspends the turner task with `vTaskSuspend()`**. Neither is ever undone. Setting a new incubation start date only resets `startEpoch`/`lastTurnEpoch`; it does not resume `hTaskTurner` and cannot clear `lockdownApplied`. The turner is only ever resumed by a *profile switch* into the incubator profile.
* **Impact:** A user who hatches a batch and starts a new one without rebooting gets: turner permanently suspended (eggs never turned → severely reduced hatch rate, discovered weeks later) and no lockdown humidity raise on day 18 of the new batch. Completely silent failure.
* **Fix applied:**
  1. `task_ui.cpp` (new-batch save path): calls `vTaskResume(hTaskTurner)` unconditionally after saving the new `startEpoch` — safe even if the turner was never suspended. Also restores `gSettings.humSetpoint` to the egg-type default (reversing the lockdown humidity raise) and persists the corrected `setHum` to NVS.
  2. `task_milestone` (`task_incubator.cpp`): tracks `lastStartEpoch`; every iteration compares the current `gSettings.startEpoch` against it. When it changes (new batch), `lockdownApplied`, `lastCheckEpoch`, and `lastMilestoneEpoch` are all reset — the new batch gets a clean milestone slate and the lockdown humidity raise will fire correctly on its day 18.
  3. `lastMilestoneEpoch` promoted from `static` local to task-scope variable so the new-batch reset can reach it.

### BUG-007 — `setRelay()` silently drops relay commands on mutex timeout ✅ FIXED
* **Severity:** High → **Fixed** (2026-06-10)
* **Location:** [globals.cpp:61-73](egg_incubator_v2/globals.cpp#L61-L73)
* **Root cause:** If `controlMutex` is not obtained within 10 ms, the function returns **without performing the `digitalWrite` at all** — the GPIO write itself, which needs no lock (it is atomic on ESP32), is gated behind the lock that only protects the `gRelayState` mirror.
* **Impact:** A safety-critical command — e.g. “heater OFF” from the over-temp latch path or the sensor-invalid gate — can be silently discarded under mutex contention. The fault latch then *believes* the heater is off while the relay stays energized; the latch path won’t retry the write because subsequent iterations short-circuit on `fault == true`… they do call `setRelay(false)` each 500 ms cycle, but each of those calls can also fail the same way under sustained contention, and `allRelaysOff()` inherits the same weakness for every channel.
* **Fix applied:** `digitalWrite(pin, …)` moved outside and before the mutex block — it now executes unconditionally on every call. The mutex is taken only to update the `gRelayState` software mirror. On mutex timeout the GPIO write still went through; `pushError(“FAULT”, “setRelay: controlMutex timeout”)` makes the contention event visible in the error queue rather than silently swallowing it.

### BUG-008 — Profile-switch suspend timeout (35 s) is shorter than worst-case task blocking; old-profile tasks can overlap the new profile ✅ FIXED
* **Severity:** High → **Fixed** (2026-06-10)
* **Location:** [egg_incubator_v2.ino:182-273](egg_incubator_v2/egg_incubator_v2.ino#L182-L273) (`switchProfile`), [config.h:92-93](egg_incubator_v2/config.h#L92-L93)
* **Root cause:** The comment claims the 35 s timeout exceeds “the longest task sleep of 30 s”, but tasks only check the suspend notification **at the top of their loop**. Worst cases: `task_pump` = 120 s pump run + 30 s sleep ≈ 150 s; `task_turner` = 120 s turn + 10 s sleep ≈ 130 s. After timeout, `switchProfile()` logs a warning and proceeds to resume the new profile anyway.
* **Impact:** (1) An in-flight turner/pump actuation from the *old* profile completes while the *new* profile’s control task is already driving relays — e.g. turner motor running in climate-chamber mode after `allRelaysOff()`. (2) The UI task (which calls `switchProfile`) freezes for up to 35 s with no display feedback and no watchdog. (3) `xTaskNotifyStateClear` on resume can also erase a *newer* pending suspend if profiles are toggled rapidly.
* **Fix applied:** Every `vTaskDelay` in `task_turner` and `task_pump` replaced with `xTaskNotifyWait(..., same_timeout)`. The suspend notification now wakes the task immediately from any blocking point — idle sleeps, bootstrap wait, and mid-actuation runs alike. On a mid-actuation wake: relay is driven OFF first (`setRelay(RELAY_TURNER/PUMP, false)`), ack bit is set, then `vTaskSuspend(NULL)` is called — the task is fully stopped within milliseconds, not up to 150 s. `TASK_SUSPEND_TIMEOUT_MS` reduced from 35 000 ms to 3 000 ms in `config.h` — now an honest safety net rather than a disguised worst-case wait.

### BUG-009 — Boot halts forever on RTC failure; OLED failure busy-spins and starves tasks ✅ FIXED
* **Severity:** High → **Fixed** (2026-06-10)
* **Location:** [egg_incubator_v2.ino:299-302](egg_incubator_v2/egg_incubator_v2.ino#L299-L302); [oled_ui.cpp:70-71](egg_incubator_v2/oled_ui.cpp#L70-L71)
* **Root cause:** `rtc.begin()` failure → infinite halt loop before any task is created. `display.begin()` failure → `while (1);` **without any delay** inside `task_ui` (priority 2, core 1).
* **Impact:** A failed/loose RTC or OLED — both on the same I²C bus, both common field failures — disables *temperature control entirely* even though the DS18B20, heater and control logic are perfectly healthy. The OLED case is worse: the busy-spin permanently starves all core-1 tasks of priority ≤ 2 (RTC task, turner, fan, pump, milestone — `task_milestone` at priority 1 never runs again), while higher-priority control tasks keep running, producing a half-alive system. For an incubator, days without heat = total loss of the batch.
* **Fix applied:**
  1. `egg_incubator_v2.ino` — RTC failure no longer halts boot. On `rtc.begin()` failure, `rtcEpochValid` is set `false` and `setup()` continues. Temperature/humidity control is fully independent of the RTC; epoch-gated logic (turner, milestones, cyclic phase) is already dormant when `rtcEpochValid=false` (BUG-005 guards). The clock can be set later via the UI or NTP.
  2. `oled_ui.cpp` — `oled_init()` return type changed from `void` to `bool`; returns `false` instead of `while(1)` on failure.
  3. `oled_ui.h` — signature updated to `bool oled_init(void)`.
  4. `task_ui.cpp` — checks `oled_init()` return value; on failure, pushes a `FAULT` error and enters `for(;;) vTaskDelay(5000)` — yields the CPU every 5 s so all other tasks on the core continue running normally.

---

## 4. Medium Findings

### BUG-010 — Over-temperature fault latch is not persisted across reboot ✅ FIXED
* **Severity:** Medium
* **Location:** [globals.cpp:28](egg_incubator_v2/globals.cpp#L28), [task_control.cpp:74-86](egg_incubator_v2/task_control.cpp#L74-L86)
* **Root cause:** `overTempFault` lives only in RAM. A power blip (or the panic-reboot the WDT would cause once BUG-001 is fixed) clears the latch.
* **Impact:** A genuine over-temp event followed by a brownout silently re-enables the heater with no operator acknowledgment — defeats the purpose of a *latched* fault.
* **Fix applied:** NVS key `"otFault"` (namespace `"incubator"`) now tracks the latch. On detection in `task_control.cpp` and `task_climate_control.cpp`, `putBool("otFault", true)` is written immediately after setting `overTempFault`. `loadSettings()` in `egg_incubator_v2.ino` reads the key on boot and restores `overTempFault` under `faultMux`. On the 3-s OK hold clear in `task_buttons.cpp`, `putBool("otFault", false)` clears the NVS flag. Each write uses a short-lived local `Preferences` instance to avoid cross-task sharing of the global `prefs` object.

### BUG-011 — Lockdown is missed entirely if the device is off (or in the other profile) on the lockdown day ✅ FIXED
* **Severity:** Medium
* **Location:** [incubator_logic.cpp:111-117](egg_incubator_v2/incubator_logic.cpp#L111-L117) (`day == lockdownDay`), [task_incubator.cpp:329](egg_incubator_v2/task_incubator.cpp#L329)
* **Root cause:** The milestone test uses exact equality on the day number, and the humidity raise happens only when the milestone task observes that day.
* **Impact:** Power outage spanning day 18 (or the user browsing in climate profile, where `task_milestone` is suspended) → humidity is never raised for hatch. Turner stopping is independently safe (`turnerActive()` uses `day < lockdownDay`), but the humidity action is one-shot.
* **Fix applied:** In `checkMilestone()` (`incubator_logic.cpp`), changed `day == (int)lockdownDay` to `day >= (int)lockdownDay && day < (int)totalDays`. The function now returns `"LOCKDOWN"` for all post-lockdown days until hatch day, so `task_milestone` can apply the action even if the device boots days late. The existing `lockdownApplied` flag in `task_milestone` prevents double-application.

### BUG-012 — “Turn Now” does not turn now ✅ FIXED
* **Severity:** Medium
* **Location:** [task_ui.cpp:884-891](egg_incubator_v2/task_ui.cpp#L884-L891); turner bootstrap [task_incubator.cpp:56-70](egg_incubator_v2/task_incubator.cpp#L56-L70)
* **Root cause:** The UI implements “Turn Now” by setting `lastTurnEpoch = 0`. But the turner task treats `lastTurn == 0` as *bootstrap*: it records the current epoch and waits a **full interval** before the first turn.
* **Impact:** The user-visible function does the opposite of its label — instead of an immediate turn, it postpones the next turn by up to 12 h. Misleading during setup/testing and for manual extra turns.
* **Fix applied:** In the “Turn Now” OK handler (`task_ui.cpp`), reads `nowEpoch` from `rtcMutex` and `intervalSec` from `settingsMutex`, then sets `lastTurnEpoch = nowEp - intervalSec`. The turner's next 10-s poll sees elapsed ≥ interval and fires immediately. The `0` bootstrap path is no longer triggered.

### BUG-013 — Date editor accepts impossible dates (e.g. 31 Feb); no day/month cross-validation ✅ FIXED
* **Severity:** Medium
* **Location:** Incubation start editor [task_ui.cpp:739-756](egg_incubator_v2/task_ui.cpp#L739-L756); RTC manual edit [task_ui.cpp:1787-1810](egg_incubator_v2/task_ui.cpp#L1787-L1810)
* **Root cause:** Day rolls 1-31 regardless of month/year. `DateTime(2026, 2, 31, …)` is undefined in RTClib (no validation) and `unixtime()` returns a wrong epoch; `rtc.adjust()` writes the invalid date into the DS1307, which may then increment nonsensically.
* **Impact:** Wrong `startEpoch` → wrong incubation day, milestones and hatch date; corrupted RTC calendar after manual set.
* **Fix applied:** Added `static uint8_t daysInMonth(int m, int y)` helper (leap-year aware) in `task_ui.cpp`. Both date editors now use it: day wraps at `daysInMonth(month, year)` instead of hardcoded 31; when month or year changes, day is clamped down if it exceeds the new month's limit. An additional defensive clamp is applied immediately before `DateTime` construction / `rtc.adjust()` on final save.
* **Recommended fix:** Clamp day to days-in-month(month, year) whenever day/month/year changes, and validate before calling `unixtime()` / `rtc.adjust()`.

### BUG-014 — WiFiManager object shared by two tasks without synchronization; `autoConnect()` blocks the UI task
* **Severity:** Medium
* **Location:** [task_wifi_manager.cpp:11, 20-66, 102-118](egg_incubator_v2/task_wifi_manager.cpp#L11-L118); called from UI at [task_ui.cpp:1545-1556](egg_incubator_v2/task_ui.cpp#L1545-L1556)
* **Root cause:** `wm.autoConnect()` / `wm.stopConfigPortal()` run in the **UI task** while `wm.process()` runs concurrently in the **wifi-manager task** (different core). WiFiManager is not thread-safe; portal start/stop races against `process()` (web-server teardown, internal state flags). `autoConnect()` also blocks the UI task for the duration of the station connect attempt (several seconds), and UI stack is only 6144 B for a WiFiManager call chain.
* **Impact:** Sporadic crashes/heap corruption around connect/disconnect; frozen UI during connect; potential UI-task stack overflow.
* **Recommended fix:** Make the wifi task the *sole* owner of `wm`; the UI should only post connect/disconnect requests (atomic flag or queue) that the wifi task executes. This also fixes the blocking and the stack concern.

### BUG-015 — Two independent reconnect drivers fight over the Wi-Fi stack
* **Severity:** Medium
* **Location:** [task_cloud.cpp:101-106](egg_incubator_v2/task_cloud.cpp#L101-L106) and [task_wifi_manager.cpp:122-127](egg_incubator_v2/task_wifi_manager.cpp#L122-L127)
* **Root cause:** Both `task_cloud` and `task_wifi_manager` call `WiFi.reconnect()` on their own 5-s cadence when disconnected (plus `WiFi.setAutoReconnect(true)` makes the driver retry on its own).
* **Impact:** Overlapping reconnect attempts can repeatedly abort each other’s association, lengthening outages and adding log noise; harder to reason about Wi-Fi state.
* **Recommended fix:** Centralize all reconnect policy in `task_wifi_manager`; `task_cloud` should only *observe* `WiFi.status()`.

### BUG-016 — NTP sync blocks the UI for up to ~7 s; timezone hardcoded to UTC+5:30
* **Severity:** Medium
* **Location:** [task_ui.cpp:1728-1761](egg_incubator_v2/task_ui.cpp#L1728-L1761)
* **Root cause:** `getLocalTime(&timeinfo, 5000)` plus a 2-s result screen run inline in the UI event handler; `configTime(19800, 0, …)` hardcodes IST with no DST/config option.
* **Impact:** Buttons dead and home screen frozen during sync; wrong wall-clock for any user outside IST (affects fixed-schedule climate mode and displayed times).
* **Recommended fix:** Run the sync in the wifi/cloud task and report back via a flag; make the UTC offset a setting.

### BUG-017 — Clock changes (manual set / NTP) are not reconciled with stored epochs
* **Severity:** Medium
* **Location:** `rtc.adjust()` call sites [task_ui.cpp:1737-1744, 1810](egg_incubator_v2/task_ui.cpp#L1737-L1810); consumers as in BUG-005
* **Root cause:** After the RTC is set backwards, `lastTurnEpoch`, `cycleStartEpoch`, `rampStepStartEpoch` and `startEpoch` may lie in the future; unsigned `now - then` underflows.
* **Impact:** Immediate spurious turner run, cyclic/ramp phase corruption, incubation day jumping (e.g. correcting a fast clock by 10 min mid-hatch triggers a turn).
* **Recommended fix:** After any `rtc.adjust()`, clamp each stored epoch to `min(stored, newNow)`; gate all elapsed-time math with a `then <= now` check.

### BUG-018 — Settings loaded from NVS are not range-validated
* **Severity:** Medium
* **Location:** [egg_incubator_v2.ino:35-88](egg_incubator_v2/egg_incubator_v2.ino#L35-L88) (`loadSettings`)
* **Root cause:** Every value is trusted as stored. Enums are cast unchecked (`(EggType)prefs.getUInt(...)`), floats are not clamped to the documented editing limits, `totalDays`/`lockdownDay`/`turnerIntervalMin` = 0 or inconsistent (`lockdownDay > totalDays`) are accepted.
* **Impact:** A corrupted or partially-written NVS (power loss during the ~38-key `saveSettings()` sequence, which is not atomic) yields out-of-range setpoints or division/modulo edge cases (`totalCycleSec == 0` is guarded, but e.g. `turnerIntervalMin = 0` → turner runs every 10 s poll; hysteresis 0 → relay chatter).
* **Recommended fix:** Clamp every loaded value to its legal range (the limits already exist in `config.h`); validate enum values; treat invalid combinations by falling back to defaults and pushing an error.

### BUG-019 — `switchProfile()` proceeds even when it failed to record the new profile
* **Severity:** Medium
* **Location:** [egg_incubator_v2.ino:186-189](egg_incubator_v2/egg_incubator_v2.ino#L186-L189)
* **Root cause:** If the 100 ms `settingsMutex` take fails, `gSettings.activeProfile` keeps the old value but the function continues suspending/resuming tasks for the *new* profile, then `saveSettings()` persists the **old** profile.
* **Impact:** Running task set and persisted/displayed profile disagree; after reboot the device starts the other profile’s tasks. Low probability, but completely silent.
* **Recommended fix:** Abort (or retry) the switch if the settings update fails; log loudly.

### BUG-020 — Single temperature sensor is a SPOF; DHT22’s temperature channel is unused for cross-checking; heater has no minimum-cycle or maximum-runtime guard
* **Severity:** Medium (industrial best practice)
* **Location:** [task_sensors.cpp](egg_incubator_v2/task_sensors.cpp), [task_control.cpp:106-129](egg_incubator_v2/task_control.cpp#L106-L129)
* **Root cause / gaps:** (1) Only the DS18B20 governs the heater; the DHT22 already measures temperature but it is never read (`dht.readTemperature()`) for plausibility checking. A drifting-but-in-range DS18B20 (condensation on the probe, partial short) reads low and the heater runs hot with no second opinion — the 39.5 °C latch only helps if the *sensor* sees the overshoot. (2) The cooler has a 30 s anti-short-cycle lockout but the heater has none, and there is no “heater continuously ON for > N minutes without temperature rise” plausibility alarm (stuck relay / failed heater / open door detection).
* **Recommended fix:** Read DHT22 temperature and alarm when |T_dht − T_ds18b20| exceeds a threshold for several minutes; add a heater max-continuous-on / no-temperature-rise watchdog; optionally add a minimum off-time for mechanical heater relays.

### BUG-021 — Heap fragmentation risk from `String` churn in the cloud task; token leaked into logs
* **Severity:** Medium
* **Location:** [task_cloud.cpp:126-168, 226-295](egg_incubator_v2/task_cloud.cpp#L126-L295)
* **Root cause:** Every 60 s, dozens of temporary `String` concatenations build the URL on the heap, plus `http.getString()` — on a 24/7 device this fragments the heap over weeks (especially alongside TLS buffers). `Serial.printf("[CLOUD] Request: %s", url)` also prints the secret token to the serial log; the token additionally travels as a GET query parameter (cached/logged by intermediaries; visible in Apps Script logs).
* **Recommended fix:** Build URLs with `snprintf` into a static/stack buffer (a `TelemetryMsg_t.url[768]` already exists — reuse that pattern for first attempts too); redact the token in logs; prefer POST with the token in a header/body.

---

## 5. Low Findings

| ID | Finding | Location | Notes / Fix |
|----|---------|----------|-------------|
| BUG-022 | Retry path treats any HTTP code > 0 (incl. 404/500) as success, while the first-attempt path checks `code != 200` — inconsistent server-error handling | [task_cloud.cpp:78-80](egg_incubator_v2/task_cloud.cpp#L78-L80) | Apply the same `code == 200` (or 2xx/3xx) criterion in both paths. |
| BUG-023 | `t == 85.0f` exact float compare rejects a legitimate 85.0 °C reading — relevant in climate profile whose valid range extends to 100 °C; also masks a real over-limit reading as “sensor invalid” instead of over-temp | [task_sensors.cpp:32](egg_incubator_v2/task_sensors.cpp#L32) | Only treat 85.0 as POR when it is the *first* reading after a bus reset, or check `ds18b20.getResolution()`-consistent raw value; in practice the >80 °C latch fires first. |
| BUG-024 | Errors are dropped silently when offline: `errorQueue` (20 deep) is drained only while Wi-Fi is connected; `pushError` drops on full with no counter, and the OLED never displays non-fault errors | [globals.cpp:48-56](egg_incubator_v2/globals.cpp#L48-L56), [task_cloud.cpp:96-99](egg_incubator_v2/task_cloud.cpp#L96-L99) | Keep a dropped-error counter; show last error / error count on a status screen. |
| BUG-025 | Dead UI code: `UI_SET_ENV_MENU` and `UI_SETTINGS_MENU` handlers are unreachable (no state transition enters them); `UI_EVT_LONGOK` is defined but never generated by `task_buttons` | [task_ui.cpp:351-448, 1474-1530](egg_incubator_v2/task_ui.cpp#L351-L1530) | Remove or wire up; dead handlers invite divergence (e.g. the stale 8-item env menu). |
| BUG-026 | `setFanSpeed()` lazy LEDC init guarded by a non-atomic `static bool initialized`, callable concurrently from UI (via `allRelaysOff`), control and fan tasks → double `ledc_*_config` race | [task_incubator.cpp:162-190](egg_incubator_v2/task_incubator.cpp#L162-L190) | Initialize LEDC once in `setup()` before tasks start. |
| BUG-027 | Fault screen shows `-999.0 C` when the sensor is invalid during an over-temp latch | [task_ui.cpp:167-173](egg_incubator_v2/task_ui.cpp#L167-L173), [oled_ui.cpp:291-294](egg_incubator_v2/oled_ui.cpp#L291-L294) | Print `--.-` when `temp_valid == false`. |
| BUG-028 | RTC fallback to compile time passes the epoch sanity gate (compile date > Nov 2023) so a wrong-but-plausible clock is treated as valid forever | [egg_incubator_v2.ino:303-306](egg_incubator_v2/egg_incubator_v2.ino#L303-L306) | Push a “clock unset” warning until the user confirms/sets time. |
| BUG-029 | `setRelay()` has no `case RELAY_FAN`; calling it for the fan would `digitalWrite` onto an LEDC-attached pin and leave `fanOn` stale — a latent footgun given the fan/relay ambiguity of BUG-002 | [globals.cpp:64-70](egg_incubator_v2/globals.cpp#L64-L70) | Either support it explicitly or assert/log on unknown pins. |
| BUG-030 | `task_rtc` reads `gRtcTime.epoch` for the sanity check *after* releasing `rtcMutex` (benign today — single writer — but fragile) | [task_rtc.cpp:20-32](egg_incubator_v2/task_rtc.cpp#L20-L32) | Use the local `now.unixtime()` value instead. |

---

## 6. Best-Practice / Safety Checklist Review

| Area | Verdict | Notes |
|------|---------|-------|
| **Watchdog** | ❌ Fail | Initialized but zero coverage (BUG-001). |
| **Fail-safe outputs** | ⚠️ Partial | Relays driven OFF first thing in `setup()` ✔; over-temp latch in both profiles ✔; sensor-invalid gate for temperature ✔ — but fan polarity contradiction (BUG-002), droppable relay writes (BUG-007), non-persistent latch (BUG-010), no degraded mode on RTC/OLED failure (BUG-009). |
| **Sensor validation** | ⚠️ Partial | DS18B20: disconnect, POR and range checks ✔. Humidity: validity flag computed but ignored by actuation, fabricated default, no stale-age limit (BUG-003). No cross-sensor plausibility (BUG-020). |
| **EEPROM/NVS usage** | ⚠️ Partial | Namespaced `Preferences`, single-key writes for high-frequency values (good for wear), read-back verify ✔. No range validation on load (BUG-018); 38-key `saveSettings()` is non-atomic; lockdown permanently overwrites the user’s humidity setpoint. |
| **Wi-Fi stability** | ⚠️ Partial | User-gated radio, non-blocking portal, `std::atomic` flags ✔. Cross-task WiFiManager races (BUG-014), duplicated reconnect logic (BUG-015). |
| **Task blocking** | ⚠️ Partial | Control loops use bounded mutex timeouts ✔. UI task performs blocking NTP (BUG-016), blocking `autoConnect` (BUG-014) and a 35 s `switchProfile` wait (BUG-008); pump/turner block 2 min during actuation, delaying suspend acks. |
| **GPIO safety** | ❌ Fail | GPIO12 strapping conflict can prevent boot (BUG-004); fan pin convention contradiction (BUG-002). Buttons on 32/33/25 with internal pull-ups ✔; relays initialized OFF before anything else ✔. |
| **Concurrency** | ✔ Mostly good | Mutex-per-domain design, event-group suspend handshake, atomics for Wi-Fi flags, rollover-safe `millis()` arithmetic throughout, fixed-size queue messages (no heap in queues). Remaining races are listed above. |
| **Secrets handling** | ✔ Good | `secrets.h` gitignored and not tracked; fallback empty defines. Token-in-URL/logs noted in BUG-021. |

---

## 7. Recommended Fix Priority

1. **Before next deployment (safety):** BUG-001 (watchdog), BUG-002 (fan polarity — verify against real hardware), BUG-003 (humidity validity gate), BUG-004 (GPIO12), BUG-007 (unconditional relay writes).
2. **Before next hatch cycle (correctness):** BUG-005/BUG-017 (epoch validity), BUG-006 (new-batch reset), BUG-010, BUG-011, BUG-012, BUG-013.
3. **Hardening / robustness:** BUG-008, BUG-009, BUG-014, BUG-015, BUG-018, BUG-020, BUG-021.
4. **Cleanup:** remaining Low items.

## 8. Positive Observations

- Centralized relay control (`setRelay`/`allRelaysOff`) with a state mirror — single choke point, easy to audit.
- Safety ordering in `setup()`: relay pins driven OFF before any peripheral init.
- Over-temperature latch with deliberate manual reset (3-s OK hold) — correct latching-alarm pattern.
- Rollover-safe `millis()` arithmetic everywhere, including the signed-difference check in the telemetry retry queue.
- Notification + event-group suspend handshake for profile switching is a sound design (only its timeout budget is wrong).
- Error throttling (per-source) prevents queue/cloud flooding.
- Fixed-size structs in queues; no heap allocation on the error path.
- DHT stale-hold and re-`begin()` recovery logic shows attention to a flaky sensor family (it just must not feed actuation unvalidated).
- Secrets isolated from version control with `__has_include` fallback.

---

*Report generated by static review on 2026-06-10. No source files were modified.*
