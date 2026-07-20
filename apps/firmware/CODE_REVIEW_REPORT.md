# Code Review Report
## Project: Egg Incubator ESP32 RTOS — Firmware v2.0.0
**Reviewer:** Claude Sonnet 4.6  
**Date:** 2026-06-08  
**Last Updated:** 2026-06-08 (F-01, F-02 fixed)  
**Branch:** `19-oled-to-clearly-show-fault-screen`  
**Scope:** Full codebase review (all `.ino`, `.cpp`, `.h` files)

---

## 1. Executive Summary

The firmware is a well-structured, dual-profile embedded controller (Egg Incubator and Climate Chamber) built on FreeRTOS for ESP32. It demonstrates strong fundamentals: dedicated per-concern tasks, proper mutex discipline over shared state, a snapshot-based NVS persistence pattern, and a clean separation between UI FSM and control logic. The profile-switching mechanism using task self-suspension via notifications is architecturally sound. The use of `std::atomic<bool>` for WiFi state and `portMUX_TYPE` for the over-temperature fault latch are correct choices.

**Both critical issues (F-01, F-02) have been fixed.** F-01 replaced the 200 ms blind delay with an EventGroup rendezvous across 7 files. F-02 applied the mutex snapshot pattern in `task_incubator.cpp`. Several moderate and minor issues remain open and are identified below.

---

## 2. Findings Overview

| ID | Severity | File | Summary |
|----|----------|------|---------|
| F-01 | ~~Critical~~ ✅ **FIXED** | `egg_incubator_v2.ino` | `switchProfile()` 200 ms window too short for tasks with 10–30 s sleep periods |
| F-02 | ~~Critical~~ ✅ **FIXED** | `task_incubator.cpp` | NVS write reads `gSettings` after mutex release — race condition |
| F-03 | ~~Moderate~~ ✅ **FIXED** | `globals.cpp` | `setRelay()` writes GPIO pin before taking `controlMutex` |
| F-04 | Moderate | `globals.cpp` | `RELAY_FAN` missing from `setRelay()` switch — silent no-op |
| F-05 | ~~Moderate~~ ✅ **FIXED** | `task_ui.cpp` | Climate home screen shows `"COOL"` during IDLE state |
| F-06 | Moderate | `oled_ui.cpp` | `"Tr:OFF"` + `"!W"` text overlap on home screen |
| F-07 | Minor | `incubator_logic.cpp` | `checkMilestone()` acquires `settingsMutex` twice unnecessarily |
| F-08 | Minor | `globals.h` / `task_buttons.cpp` | `UI_EVT_LONGOK` defined but never generated — dead enum value |
| F-09 | Minor | `task_ui.cpp` | `UI_SETTINGS_MENU` state handler is unreachable — dead code |
| F-10 | Minor | `task_ui.cpp` | NTP GMT offset hardcoded to UTC+5:30 (India only) |
| F-11 | Minor | `task_wifi_manager.cpp` | `wifi_request_connect()` can block `task_ui` thread |
| F-12 | Minor | `task_ui.cpp` | Pump minimum duration (5 s) is a magic number not in `config.h` |
| F-13 | Minor | `task_rtc.cpp` | RTC epoch sanity threshold is a magic number not in `config.h` |
| F-14 | Cosmetic | `incubator_logic.cpp` | `EGG_CUSTOM` silently falls through to chicken candle day |
| F-15 | Cosmetic | `egg_incubator_v2.ino` | Boot-time direct `vTaskSuspend()` vs. runtime notify-suspend inconsistency undocumented |

---

## 3. Detailed Findings

---

### F-01 — CRITICAL: `switchProfile()` suspension window is insufficient

**File:** `egg_incubator_v2.ino`, lines 179–230  
**Function:** `switchProfile()`

**Description:**  
After sending `TASK_CMD_SUSPEND` to the outgoing profile's tasks, the function waits only 200 ms before resuming the incoming profile's tasks. Tasks self-suspend by checking the notification at the **top of their loop** — after waking from any `vTaskDelay`. The suspension window must therefore be at least as long as the longest sleep in the tasks being suspended.

| Task | Worst-case sleep before self-suspend check |
|------|---------------------------------------------|
| `task_temperature_control` | 500 ms |
| `task_climate_control` | 500 ms |
| `task_turner` | **10 000 ms** |
| `task_pump` | **30 000 ms** |

With a 200 ms window, `task_turner` and `task_pump` will not see the `TASK_CMD_SUSPEND` notification before the new profile's tasks are already resumed. During that overlap window (up to ~30 seconds), both the old and new profile's control tasks are running concurrently. While `allRelaysOff()` is called first, the old-profile task can immediately re-energize relays before it reaches its suspend point.

**Risk:** Relay state corruption and unintended actuation during profile switching.

**Applied Fix (2026-06-08):**  
EventGroup rendezvous implemented. The 200 ms fixed delay was replaced with a deterministic acknowledgement handshake.

**Files changed:** `config.h`, `globals.h`, `globals.cpp`, `egg_incubator_v2.ino`, `task_control.cpp`, `task_climate_control.cpp`, `task_incubator.cpp`

**Approach:**
- `TASK_SUSPEND_BIT_*` constants (one per suspendable task) and combined masks `TASK_SUSPEND_BITS_INCUBATOR` / `TASK_SUSPEND_BITS_CLIMATE` added to `config.h`. Timeout constant `TASK_SUSPEND_TIMEOUT_MS = 35000` (exceeds the 30 s sleep of `task_pump`).
- `suspendAckGroup` (`EventGroupHandle_t`) declared in `globals.h` / `globals.cpp`, created in `setup()` via `xEventGroupCreate()`.
- Each of the 6 suspendable tasks sets its ack bit immediately before `vTaskSuspend(NULL)`:

```cpp
xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_XXX);
vTaskSuspend(NULL);
```

- `switchProfile()` replaced `vTaskDelay(200)` with `xEventGroupClearBits` + `xEventGroupWaitBits` (35 s timeout, clear-on-exit):

```cpp
EventBits_t got = xEventGroupWaitBits(
    suspendAckGroup, waitBits, pdTRUE, pdTRUE,
    pdMS_TO_TICKS(TASK_SUSPEND_TIMEOUT_MS));
if ((got & waitBits) != waitBits)
    Serial.println("[SYSTEM] WARNING: tasks did not ack suspend in time");
```

- **Edge case handled:** Each task is checked with `eTaskGetState() != eSuspended` before sending `TASK_CMD_SUSPEND`. Tasks already suspended (e.g., `task_turner` suspended at lockdown by `task_milestone`) are excluded from the wait mask, avoiding a spurious 35 s timeout on normal lockdown → profile-switch flows.

---

### F-02 — CRITICAL: Race condition — NVS write reads `gSettings` after mutex release

**File:** `task_incubator.cpp`, lines 326–337  
**Function:** `task_milestone()`

**Description:**  
After the mutex is released, `gSettings.humSetpoint` is read again without mutex protection for the NVS write:

```cpp
// Mutex released here ↓
xSemaphoreGive(settingsMutex);

// gSettings accessed WITHOUT mutex ↓
prefs.putFloat("setHum", gSettings.humSetpoint);
```

Between releasing the mutex and the `prefs.putFloat` call, `task_ui` may modify `gSettings.humSetpoint` (e.g., if the user adjusts humidity setpoint concurrently). **Risk:** NVS stores a stale or incorrect value, causing wrong behaviour after a power cycle.

**Applied Fix (2026-06-08):**  
Mutex snapshot pattern applied.

**Files changed:** `task_incubator.cpp`

**Approach:** `float humToSave = 0.0f;` declared before the mutex block. The setpoint is captured inside the critical section before `xSemaphoreGive`, and the NVS write uses the local snapshot:

```cpp
float humToSave = 0.0f;
if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    gSettings.humSetpoint = gSettings.lockdownHumidity;
    humToSave = gSettings.humSetpoint;  // snapshot before release
    xSemaphoreGive(settingsMutex);
}
{
    Preferences prefs;
    prefs.begin("incubator", false);
    prefs.putFloat("setHum", humToSave);  // safe — uses snapshot
    prefs.end();
}
```

**Scope clarification:** `climate_logic.cpp` was reviewed as part of this fix. The NVS writes there use the local variable `nowEpoch` (a function parameter) and stack-local `nextIdx`/`stepIdx` — not `gSettings` fields directly — so no race exists in that file.

---

### F-03 — MODERATE: `setRelay()` writes GPIO before acquiring `controlMutex`

**File:** `globals.cpp`, lines 60–73  
**Function:** `setRelay()`

**Description:**  
The physical pin is changed before `controlMutex` is taken. Any concurrent reader of `gRelayState` (e.g., `task_cloud` building its telemetry URL) that acquires the mutex during this brief window will read a state that does not match the physical hardware.

```cpp
void setRelay(uint8_t pin, bool on) {
    digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);  // ← hardware changes here
    if (xSemaphoreTake(controlMutex, ...)) {        // ← state mirror updated here
        ...
    }
}
```

**Risk:** Telemetry or display shows incorrect relay state for one task tick. Not a safety risk because the physical state is always authoritative.

**Applied Fix (2026-06-08):**  
`xSemaphoreTake` moved above `digitalWrite`. Both the GPIO write and the `gRelayState` mirror update now happen atomically under `controlMutex`. If the mutex times out (10 ms), neither the pin nor the mirror is changed — consistent no-op rather than a hardware/software state split.

**Files changed:** `globals.cpp`

---

### F-04 — MODERATE: `RELAY_FAN` is absent from `setRelay()` switch

**File:** `globals.cpp`, lines 64–71  
**Function:** `setRelay()`

**Description:**  
The `switch(pin)` inside `setRelay()` has no `case RELAY_FAN:`. If any code path calls `setRelay(RELAY_FAN, ...)`, the GPIO will be physically toggled but `gRelayState.fanOn` will **not** be updated. `setFanSpeed()` correctly handles the fan state mirror separately, and `allRelaysOff()` correctly calls `setFanSpeed(0)` rather than `setRelay(RELAY_FAN, false)`. However the absence of a `RELAY_FAN` case is a silent latent defect — any future code that passes `RELAY_FAN` to `setRelay()` will produce a state inconsistency with no warning.

**Recommended Fix:**  
Add either a `case RELAY_FAN:` that asserts/logs an error ("use setFanSpeed() for fan control"), or add a comment above the `switch` documenting the intentional omission.

---

### F-05 — MODERATE: Climate home screen shows `"COOL"` during IDLE

**File:** `task_ui.cpp`, lines 217–221  
**Function:** `task_ui()` — `UI_HOME` refresh block

**Description:**  
For both `CLIMATE_FIXED_SCHEDULE` and `CLIMATE_CYCLIC` modes, the phase label is derived solely from `rs.heaterOn`:

```cpp
snprintf(phase, sizeof(phase), rs.heaterOn ? "HEAT" : "COOL");
```

When neither the heater nor the cooler is active (temperature within hysteresis band), this displays `"COOL"` instead of `"IDLE"`. Note that `task_cloud` correctly computes the phase as `"HEAT"`, `"COOL"`, or `"IDLE"` using both relay states — the UI should use the same logic.

**Applied Fix (2026-06-08):**  
Both `CLIMATE_FIXED_SCHEDULE` and `CLIMATE_CYCLIC` branches now check both relay states before defaulting to `"IDLE"`. `CLIMATE_RAMP` was already correct (`"RAMP N"`).

**Files changed:** `task_ui.cpp`

```cpp
if      (rs.heaterOn) snprintf(phase, sizeof(phase), "HEAT");
else if (rs.coolerOn) snprintf(phase, sizeof(phase), "COOL");
else                  snprintf(phase, sizeof(phase), "IDLE");
```

---

### F-06 — MODERATE: Text overlap on the incubator home screen

**File:** `oled_ui.cpp`, lines 178–183  
**Function:** `oled_show_home_incubator()`

**Description:**  
At y=55, the turner state label starts at x=64. "Tr:OFF" is 6 characters, each approximately 6 pixels wide, ending at approximately x=100. The `"!W"` (WiFi offline indicator) is placed at x=96, which overlaps the trailing character(s) of "OFF".

```
x: 64                  96 100
   "Tr:OFF"            "!W"
            ↑ overlap zone ↑
```

This only manifests when the turner relay is OFF **and** WiFi is disconnected simultaneously — an uncommon but possible state during a fault or portal timeout.

**Recommended Fix:**  
Move the `"!W"` cursor to x=104, or reduce "OFF" to "0" (single char) in this position, or use the `"ON "` trailing space already present for `turnerOn` to keep column alignment and shift `"!W"` rightward.

---

### F-07 — MINOR: `checkMilestone()` acquires `settingsMutex` twice

**File:** `incubator_logic.cpp`, lines 87–99  
**Function:** `checkMilestone()`

**Description:**  
The function takes `settingsMutex` once to read `startEpoch`, `lockdownDay`, and `totalDays`, releases it, then immediately takes it again to read `eggType`. Both reads can be combined into a single mutex acquisition, reducing lock contention and eliminating the gap between the two reads (during which the values could theoretically change).

**Recommended Fix:**  
Read all four fields — `startEpoch`, `lockdownDay`, `totalDays`, and `eggType` — inside a single mutex scope.

---

### F-08 — MINOR: `UI_EVT_LONGOK` defined but never generated

**File:** `globals.h` line 138; `task_buttons.cpp`

**Description:**  
`UI_EVT_LONGOK` is declared in the `UiEvent` enum and has a case in `uiEventName()`. However, `task_buttons` never pushes this event to `uiEventQueue`. The long-press logic on the OK button only resets the over-temperature fault internally; it does not generate a `UI_EVT_LONGOK` for the UI state machine. The enum value and its `uiEventName()` entry are dead code.

**Recommended Fix:**  
Either implement the LONGOK event (push it to `uiEventQueue` after the fault reset to let the UI react), or remove the enum value and the `uiEventName()` case.

---

### F-09 — MINOR: `UI_SETTINGS_MENU` state is unreachable (dead code)

**File:** `task_ui.cpp`, lines 1470–1526  
**Function:** `task_ui()`

**Description:**  
The navigation tree is:
```
HOME → MAIN_MENU → { EGG_INCUBATOR_MENU | CLIMATE_CHAMBER_MENU | SYSTEM_MENU }
```
No transition in the entire FSM sets `uiState = UI_SETTINGS_MENU`. The state handler at line 1470 and the corresponding `oled_show_settings_menu()` function in `oled_ui.cpp` are legacy code from a prior navigation design and are never executed.

**Recommended Fix:**  
Remove the `UI_SETTINGS_MENU` state handler from `task_ui.cpp`, the `oled_show_settings_menu()` function from `oled_ui.cpp`/`oled_ui.h`, and the `UI_SETTINGS_MENU` enum value from `globals.h`. Also remove the associated menu-index enum `SettingsMenuItem` if it is no longer used.

---

### F-10 — MINOR: NTP GMT offset hardcoded to UTC+5:30

**File:** `task_ui.cpp`, line 1729

**Description:**  
```cpp
configTime(19800, 0, "pool.ntp.org");   // 19800 seconds = +5:30 (India Standard Time)
```
This value is a magic number with no documentation at the call site. Any deployment outside India will silently set the RTC to the wrong time after a WiFi sync, causing incorrect incubation day calculations and schedule windows.

**Recommended Fix:**  
Define a named constant in `config.h`:
```cpp
#define NTP_GMT_OFFSET_SEC   19800   // UTC+5:30 — adjust for your timezone
#define NTP_DST_OFFSET_SEC   0
```
This surfaces the configuration to the user, makes it easy to change, and documents the intent.

---

### F-11 — MINOR: `wifi_request_connect()` can block `task_ui`

**File:** `task_wifi_manager.cpp`, lines 34–47  
**Function:** `wifi_request_connect()`

**Description:**  
`wm.autoConnect()` first attempts the stored credentials synchronously before falling back to the captive portal (non-blocking). If the stored SSID is no longer available, the connection attempt can take several seconds before timing out. Since this function is called directly from `task_ui` (triggered by the user selecting "Connect"), the UI task is blocked for that duration: the OLED freezes and button presses are not processed.

**Risk:** Unresponsive UI for up to ~10 seconds; user may interpret the device as crashed.

**Recommended Fix:**  
Move the `wm.autoConnect()` call into `task_wifi_manager` by posting a connection request via a queue or flag, and have `task_ui` transition immediately to a "Connecting…" splash screen that periodically checks `wifiUserEnabled` / `WiFi.status()` for the result.

---

### F-12 — MINOR: Pump minimum duration is a magic number

**File:** `task_ui.cpp`, lines 1244–1246

**Description:**  
```cpp
editPumpDur = (uint16_t)max((int)editPumpDur - 5, 5);  // lower bound = 5s
```
The value `5` seconds is not defined in `config.h`. There is a `DEFAULT_PUMP_DURATION_SEC` (10 s) but no `PUMP_MIN_DURATION_SEC`. If the minimum is later changed, only the UI file is affected and the discrepancy may go unnoticed.

**Recommended Fix:**  
Add `#define PUMP_MIN_DURATION_SEC 5` to `config.h` and reference it here and in any validation logic.

---

### F-13 — MINOR: RTC epoch sanity threshold is a magic number

**File:** `task_rtc.cpp`, line 24

**Description:**  
```cpp
if (gRtcTime.epoch < 1700000000UL) {   // hard-coded epoch for Nov 2023
```
The threshold value and its corresponding calendar date are only documented in an adjacent comment, not in `config.h`. Code that references a magic timestamp without a named constant is fragile and harder to adjust if the threshold needs updating.

**Recommended Fix:**  
```c
// config.h
#define RTC_MIN_VALID_EPOCH  1700000000UL   // 2023-11-14 — below this = RTC not set
```

---

### F-14 — COSMETIC: `EGG_CUSTOM` silently inherits chicken candle day

**File:** `incubator_logic.cpp`, lines 103–106  
**Function:** `checkMilestone()`

**Description:**  
The candle day lookup defaults to `CHICKEN_CANDLE_DAY` (day 7) for any egg type not explicitly handled:
```cpp
int candleDay = CHICKEN_CANDLE_DAY;
if      (eggType == EGG_DUCK)  candleDay = DUCK_CANDLE_DAY;
else if (eggType == EGG_QUAIL) candleDay = QUAIL_CANDLE_DAY;
// EGG_CUSTOM falls through to CHICKEN_CANDLE_DAY
```
A user with a custom egg type and a 35-day incubation period still receives a candling reminder on day 7. This is not incorrect per se, but the behaviour is undocumented and may be surprising.

**Recommended Fix:**  
For `EGG_CUSTOM`, either skip the candle milestone entirely, or add a `customCandleDay` field to `Settings_t` that the user can configure.

---

### F-15 — COSMETIC: Boot-time direct `vTaskSuspend()` vs. runtime notify-suspend undocumented

**File:** `egg_incubator_v2.ino`, lines 338–346

**Description:**  
At boot, `vTaskSuspend()` is called directly on tasks that do not belong to the active profile. At runtime, `switchProfile()` uses the notify-based self-suspension mechanism instead, specifically to avoid deadlocks (per the M-6 fix comment). The boot-time direct suspension is safe because no code has run in those tasks yet (they have not acquired any mutex), but this reasoning is not documented in a comment. A future developer modifying task startup order could inadvertently break this invariant.

**Recommended Fix:**  
Add a comment at the boot suspend block:
```cpp
// Direct vTaskSuspend() is safe here: tasks have been created but their
// loop bodies have not yet executed, so no mutex is held. At runtime,
// use switchProfile() which relies on task-notification self-suspension
// to avoid mid-critical-section deadlocks.
```

---

## 4. Positive Observations

The following aspects of the codebase are well implemented and worth noting:

- **Over-temperature fault latch** — Correctly uses `portMUX_TYPE` critical section for atomic set/clear across cores. Fault state is checked at the top of every control loop and in the UI task, ensuring hardware is always safe-off before any display or button processing.
- **Self-suspending task notification pattern** — The solution to the M-6 deadlock (tasks notifying themselves to suspend at a safe point rather than being suspended externally) is the correct RTOS design for this use case.
- **`saveSettings()` snapshot pattern** — Taking a local `snap = gSettings` under `settingsMutex` and then writing to NVS outside the mutex is the correct way to avoid holding the mutex during slow NVS flash operations.
- **`pushError()` non-blocking** — Using `xQueueSend(..., 0)` and silently dropping on full queue is appropriate for an error reporting path that must never block a control task.
- **DHT recovery mechanism** — Re-calling `dht.begin()` every 5th consecutive failure is a pragmatic bus-recovery strategy.
- **Cooler lockout timer** — `COOLER_LOCKOUT_SEC` prevents short-cycling of the cooling compressor, protecting the hardware correctly.
- **`secrets.h` guard** — The `__has_include` guard with empty-string defaults ensures cloud credentials are never accidentally compiled in when the secrets file is absent, and the file is gitignored.
- **`std::atomic<bool>` for WiFi flags** — Replacing `volatile bool` with `std::atomic<bool>` for `wifiUserEnabled` and `wifiPortalActive` (M-8 fix) is the correct C++ approach for cross-core flag sharing without a mutex.

---

## 5. Priority Fix Order

The following order is recommended for addressing the findings:

1. ~~**F-01** — Profile switch suspension window~~ ✅ Fixed 2026-06-08
2. ~~**F-02** — Mutex-release race before NVS write~~ ✅ Fixed 2026-06-08
3. ~~**F-03** — `setRelay()` mutex order~~ ✅ Fixed 2026-06-08
4. ~~**F-05** — Climate IDLE label~~ ✅ Fixed 2026-06-08
5. **F-06** — Text overlap on home screen (display defect)
6. **F-04** — `setRelay()` missing FAN case (defensive hardening)
6. **F-10** — NTP timezone constant (portability)
7. **F-08 / F-09** — Dead code removal (code cleanliness)
8. **F-11** — WiFi connect blocking UI (UX improvement)
9. **F-07 / F-12 / F-13 / F-14 / F-15** — Minor quality improvements

---

## 6. Fix Log

| Date | ID | Commit context | Summary |
|------|----|---------------|---------|
| 2026-06-08 | F-01 | branch `19-oled-to-clearly-show-fault-screen` | Replaced 200 ms blind delay in `switchProfile()` with EventGroup rendezvous. 7 files changed. Each suspendable task acks suspension via `xEventGroupSetBits` before `vTaskSuspend(NULL)`. Already-suspended tasks skipped via `eTaskGetState()` check. |
| 2026-06-08 | F-02 | branch `19-oled-to-clearly-show-fault-screen` | Applied mutex snapshot pattern in `task_milestone()`. `humToSave` captured inside `settingsMutex` scope; `prefs.putFloat` uses snapshot. 1 file changed. |
| 2026-06-08 | F-03 | branch `19-oled-to-clearly-show-fault-screen` | Moved `xSemaphoreTake` above `digitalWrite` in `setRelay()`. GPIO write and `gRelayState` mirror now atomic under `controlMutex`. 1 file changed. |
| 2026-06-08 | F-05 | branch `19-oled-to-clearly-show-fault-screen` | Fixed climate phase label: both FIXED_SCHEDULE and CYCLIC branches now check `rs.heaterOn` then `rs.coolerOn` then default to `"IDLE"`. 1 file changed. |

---

*End of Report*
