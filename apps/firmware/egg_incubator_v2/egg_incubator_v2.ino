#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>
#include "esp_task_wdt.h"

#include "RTClib.h"

#include "config.h"
#include "globals.h"
#include "task_sensors.h"
#include "task_control.h"
#include "task_rtc.h"
#include "task_buttons.h"
#include "task_ui.h"
#include "task_cloud.h"
#include "task_mqtt.h"
#include "task_incubator.h"
#include "task_wifi_manager.h"

// Explicit C++ prototypes — required because these .cpp files include Arduino.h
// after their own header, causing C-linkage/C++-linkage mismatch with this TU.
void task_rtc(void* pvParameters);
void task_cloud(void* pvParameters);
void task_mqtt(void* pvParameters);
void task_wifi_manager(void* pvParameters);

// ─────────────────────────────────────────────────────────────────────────────
// HARDWARE OBJECTS (extern'd where needed)
// ─────────────────────────────────────────────────────────────────────────────
RTC_DS1307  rtc;
Preferences prefs;

// ─────────────────────────────────────────────────────────────────────────────
// loadSettings — reads all persisted values from NVS into gSettings
// ─────────────────────────────────────────────────────────────────────────────
void loadSettings(void) {
    prefs.begin("incubator", true);  // read-only

    bool nvsBad = false;

    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {

        gSettings.activeProfile       = (ProfileType)prefs.getUInt("profile",   PROFILE_EGG_INCUBATOR);
        gSettings.controlMode         = (ControlMode)prefs.getUInt("ctrlMode",  MODE_AUTO);

        gSettings.tempSetpoint        = prefs.getFloat("setTemp",   DEFAULT_TEMP_SETPOINT);
        gSettings.tempHysteresis      = prefs.getFloat("hyst",      DEFAULT_TEMP_HYSTERESIS);
        gSettings.humSetpoint         = prefs.getFloat("setHum",    DEFAULT_HUM_SETPOINT);
        gSettings.humHysteresis       = prefs.getFloat("humHyst",   DEFAULT_HUM_HYSTERESIS);

        gSettings.heaterManualOn      = prefs.getBool("heatMan",    false);
        gSettings.coolerManualOn      = prefs.getBool("coolMan",    false);

        // Egg incubator
        gSettings.eggType             = (EggType)prefs.getUInt("eggType",  EGG_CHICKEN);
        gSettings.totalDays           = prefs.getUInt("totalDays", CHICKEN_TOTAL_DAYS);
        gSettings.lockdownDay         = prefs.getUInt("lockdownDay",CHICKEN_LOCKDOWN_DAY);
        gSettings.lockdownHumidity    = prefs.getFloat("lockdownHum",DEFAULT_LOCKDOWN_HUM);
        gSettings.startEpoch          = prefs.getULong("startEpoch", 0);
        gSettings.turnerIntervalMin   = prefs.getUInt("turnerIntv", DEFAULT_TURNER_INTERVAL_MIN);
        gSettings.turnerDurationSec   = prefs.getUInt("turnerDur",  DEFAULT_TURNER_DURATION_SEC);
        gSettings.lastTurnEpoch       = prefs.getULong("lastTurn",  0);
        gSettings.fanSpeedPercent     = prefs.getUInt("fanSpeed",   DEFAULT_FAN_SPEED_PERCENT);
        gSettings.pumpDurationSec     = prefs.getUInt("pumpDur",    DEFAULT_PUMP_DURATION_SEC);

        // Climate chamber
        gSettings.climateMode         = (ClimateModeType)prefs.getUInt("climMode", CLIMATE_FIXED_SCHEDULE);
        gSettings.schedStartHour      = prefs.getUInt("schedStart", DEFAULT_SCHED_START_HOUR);
        gSettings.schedEndHour        = prefs.getUInt("schedEnd",   DEFAULT_SCHED_END_HOUR);
        gSettings.heatPeriodMin       = prefs.getULong("heatPeriod",DEFAULT_CLIMATE_HEAT_PERIOD_MIN);
        gSettings.coolPeriodMin       = prefs.getULong("coolPeriod",DEFAULT_CLIMATE_COOL_PERIOD_MIN);
        gSettings.cycleStartEpoch     = prefs.getULong("cycleStart",0);
        gSettings.rampCount           = prefs.getUInt("rampCount",  0);
        gSettings.rampStepIdx         = prefs.getUInt("rampIdx",    0);
        gSettings.rampStepStartEpoch  = prefs.getULong("rampStart", 0);

        for (int i = 0; i < CLIMATE_MAX_RAMP_STEPS; i++) {
            char kd[16], kt[16];
            snprintf(kd, sizeof(kd), "rampDur%d", i);
            snprintf(kt, sizeof(kt), "rampTmp%d", i);
            gSettings.rampSteps[i].durationMin = prefs.getUInt(kd,  60);
            gSettings.rampSteps[i].targetTemp  = prefs.getFloat(kt, DEFAULT_TEMP_SETPOINT);
        }

        // ── Range-validate every loaded value; fall back to defaults on bad data ──
        #define CLAMP(v, lo, hi, def) do { if ((v) < (lo) || (v) > (hi)) { (v) = (def); nvsBad = true; } } while(0)
        #define CLAMPUI(v, lo, hi, def) do { if ((v) < (uint32_t)(lo) || (v) > (uint32_t)(hi)) { (v) = (def); nvsBad = true; } } while(0)

        // Enums — anything outside known values → default
        if (gSettings.activeProfile > PROFILE_CLIMATE_CHAMBER)
            { gSettings.activeProfile = PROFILE_EGG_INCUBATOR; nvsBad = true; }
        if (gSettings.controlMode > MODE_MANUAL)
            { gSettings.controlMode = MODE_AUTO; nvsBad = true; }
        if (gSettings.eggType >= EGG_TYPE_COUNT)
            { gSettings.eggType = EGG_CHICKEN; nvsBad = true; }
        if (gSettings.climateMode > CLIMATE_RAMP)
            { gSettings.climateMode = CLIMATE_FIXED_SCHEDULE; nvsBad = true; }

        // Floats
        CLAMP(gSettings.tempSetpoint,     TEMP_SETPOINT_MIN, TEMP_SETPOINT_MAX, DEFAULT_TEMP_SETPOINT);
        CLAMP(gSettings.tempHysteresis,   TEMP_HYST_MIN,     TEMP_HYST_MAX,     DEFAULT_TEMP_HYSTERESIS);
        CLAMP(gSettings.humSetpoint,      HUM_SETPOINT_MIN,  HUM_SETPOINT_MAX,  DEFAULT_HUM_SETPOINT);
        CLAMP(gSettings.humHysteresis,    HUM_HYST_MIN,      HUM_HYST_MAX,      DEFAULT_HUM_HYSTERESIS);
        CLAMP(gSettings.lockdownHumidity, HUM_SETPOINT_MIN,  HUM_SETPOINT_MAX,  DEFAULT_LOCKDOWN_HUM);

        // Turner — minimum 1 to avoid divide-by-zero / runaway
        CLAMPUI(gSettings.turnerIntervalMin,  TURNER_MIN_INTERVAL_MIN,  TURNER_MAX_INTERVAL_MIN,  DEFAULT_TURNER_INTERVAL_MIN);
        CLAMPUI(gSettings.turnerDurationSec,  TURNER_MIN_DURATION_SEC,  TURNER_MAX_DURATION_SEC,  DEFAULT_TURNER_DURATION_SEC);

        // Fan / pump
        if (gSettings.fanSpeedPercent  > 100) { gSettings.fanSpeedPercent  = DEFAULT_FAN_SPEED_PERCENT;  nvsBad = true; }
        if (gSettings.pumpDurationSec  == 0)  { gSettings.pumpDurationSec  = DEFAULT_PUMP_DURATION_SEC;  nvsBad = true; }

        // Incubation days — must be non-zero and consistent
        if (gSettings.totalDays == 0)   { gSettings.totalDays = CHICKEN_TOTAL_DAYS; nvsBad = true; }
        if (gSettings.lockdownDay == 0 || gSettings.lockdownDay >= gSettings.totalDays)
            { gSettings.lockdownDay = gSettings.totalDays > 3 ? gSettings.totalDays - 3 : 1; nvsBad = true; }

        // Schedule hours
        if (gSettings.schedStartHour > 23) { gSettings.schedStartHour = DEFAULT_SCHED_START_HOUR; nvsBad = true; }
        if (gSettings.schedEndHour   > 23) { gSettings.schedEndHour   = DEFAULT_SCHED_END_HOUR;   nvsBad = true; }

        // Cyclic periods — 0 would cause divide-by-zero in cyclicInHeatPhase()
        if (gSettings.heatPeriodMin == 0) { gSettings.heatPeriodMin = DEFAULT_CLIMATE_HEAT_PERIOD_MIN; nvsBad = true; }
        if (gSettings.coolPeriodMin == 0) { gSettings.coolPeriodMin = DEFAULT_CLIMATE_COOL_PERIOD_MIN; nvsBad = true; }

        // Ramp step index bounds
        if (gSettings.rampCount > CLIMATE_MAX_RAMP_STEPS) { gSettings.rampCount = 0; nvsBad = true; }
        if (gSettings.rampStepIdx >= gSettings.rampCount && gSettings.rampCount > 0)
            { gSettings.rampStepIdx = 0; nvsBad = true; }

        #undef CLAMP
        #undef CLAMPUI

        xSemaphoreGive(settingsMutex);
    }

    if (nvsBad) {
        pushError("NVS_CORRUPT", "Settings out of range — defaults applied");
        Serial.println("[NVS] Warning: one or more settings were out of range and reset to defaults");
    }

    // Restore over-temperature fault latch — survives power loss / WDT reboot
    if (prefs.getBool("otFault", false)) {
        portENTER_CRITICAL(&faultMux);
        overTempFault = true;
        portEXIT_CRITICAL(&faultMux);
        Serial.println("[NVS] Over-temp fault latch restored — hold OK 3 s to clear");
    }

    prefs.end();
    Serial.println("[NVS] Settings loaded");
    Serial.printf("[NVS] startEpoch loaded = %lu\n", (unsigned long)gSettings.startEpoch);
}

// ─────────────────────────────────────────────────────────────────────────────
// saveSettings — writes gSettings to NVS (called only on confirmed UI saves)
// ─────────────────────────────────────────────────────────────────────────────
void saveSettings(void) {
    Settings_t snap = {};
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        snap = gSettings;
        xSemaphoreGive(settingsMutex);
    }

    prefs.begin("incubator", false);  // write mode

    prefs.putUInt("profile",    (uint32_t)snap.activeProfile);
    prefs.putUInt("ctrlMode",   (uint32_t)snap.controlMode);

    prefs.putFloat("setTemp",   snap.tempSetpoint);
    prefs.putFloat("hyst",      snap.tempHysteresis);
    prefs.putFloat("setHum",    snap.humSetpoint);
    prefs.putFloat("humHyst",   snap.humHysteresis);

    prefs.putBool("heatMan",    snap.heaterManualOn);
    prefs.putBool("coolMan",    snap.coolerManualOn);

    prefs.putUInt("eggType",    (uint32_t)snap.eggType);
    prefs.putUInt("totalDays",  snap.totalDays);
    prefs.putUInt("lockdownDay",snap.lockdownDay);
    prefs.putFloat("lockdownHum",snap.lockdownHumidity);
    prefs.putULong("startEpoch",snap.startEpoch);
    prefs.putUInt("turnerIntv", snap.turnerIntervalMin);
    prefs.putUInt("turnerDur",  snap.turnerDurationSec);
    prefs.putULong("lastTurn",  snap.lastTurnEpoch);
    prefs.putUInt("fanSpeed",   snap.fanSpeedPercent);
    prefs.putUInt("pumpDur",    snap.pumpDurationSec);

    prefs.putUInt("climMode",   (uint32_t)snap.climateMode);
    prefs.putUInt("schedStart", snap.schedStartHour);
    prefs.putUInt("schedEnd",   snap.schedEndHour);
    prefs.putULong("heatPeriod",snap.heatPeriodMin);
    prefs.putULong("coolPeriod",snap.coolPeriodMin);
    prefs.putULong("cycleStart",snap.cycleStartEpoch);
    prefs.putUInt("rampCount",  snap.rampCount);
    prefs.putUInt("rampIdx",    snap.rampStepIdx);
    prefs.putULong("rampStart", snap.rampStepStartEpoch);

    for (int i = 0; i < CLIMATE_MAX_RAMP_STEPS; i++) {
        char kd[16], kt[16];
        snprintf(kd, sizeof(kd), "rampDur%d", i);
        snprintf(kt, sizeof(kt), "rampTmp%d", i);
        prefs.putUInt(kd,  snap.rampSteps[i].durationMin);
        prefs.putFloat(kt, snap.rampSteps[i].targetTemp);
    }

    prefs.end();
    // Verify by reading back the stored value (diagnostic)
    prefs.begin("incubator", true);
    uint32_t verify = prefs.getULong("startEpoch", 0);
    prefs.end();
    Serial.printf("[NVS] Settings saved, startEpoch written=%lu verify=%lu\n", (unsigned long)snap.startEpoch, (unsigned long)verify);
}

// ─────────────────────────────────────────────────────────────────────────────
// factoryReset — wipes NVS and reboots
// ─────────────────────────────────────────────────────────────────────────────
void factoryReset(void) {
    allRelaysOff();  // safe hardware state before NVS wipe
    prefs.begin("incubator", false);
    prefs.clear();
    prefs.end();
    Serial.println("[NVS] Factory reset — restarting");
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP.restart();
}

// ─────────────────────────────────────────────────────────────────────────────
// switchProfile — safe profile switch using task-notification based suspend
//
// Each suspendable task checks for TASK_CMD_SUSPEND at the TOP of its loop
// (outside any mutex/critical-section), sets its ack bit in suspendAckGroup,
// then calls vTaskSuspend(NULL) itself.
//
// Flow:
//   1. allRelaysOff() — safe hardware state immediately
//   2. Persist new profile to gSettings
//   3. For each outgoing task that is currently running (not already suspended
//      e.g. turner suspended by lockdown): send TASK_CMD_SUSPEND and add its
//      bit to the wait mask. Already-suspended tasks are skipped.
//   4. xEventGroupWaitBits() — block until every notified task has acked
//      (i.e. set its bit just before vTaskSuspend). Timeout after
//      TASK_SUSPEND_TIMEOUT_MS (35 s > longest task sleep of 30 s).
//   5. Resume the tasks that belong to the new profile, clearing any stale
//      TASK_CMD_SUSPEND notification so they don't re-suspend instantly.
// ─────────────────────────────────────────────────────────────────────────────
void switchProfile(ProfileType newProfile) {
    allRelaysOff();
    Serial.println("[SYSTEM] Profile switch requested — notifying tasks");

    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("[SYSTEM] ERROR: switchProfile — settingsMutex timeout; switch aborted");
        pushError("FAULT", "switchProfile: mutex timeout — profile NOT switched");
        return;
    }
    gSettings.activeProfile = newProfile;
    xSemaphoreGive(settingsMutex);

    if (newProfile == PROFILE_EGG_INCUBATOR) {
        // Only notify the climate task if it is actually running
        EventBits_t waitBits = 0;
        xEventGroupClearBits(suspendAckGroup, TASK_SUSPEND_BITS_CLIMATE);
        if (hTaskClimateControl != nullptr &&
            eTaskGetState(hTaskClimateControl) != eSuspended) {
            xTaskNotify(hTaskClimateControl, TASK_CMD_SUSPEND, eSetValueWithOverwrite);
            waitBits |= TASK_SUSPEND_BIT_CLIM_CTRL;
        }

        if (waitBits) {
            EventBits_t got = xEventGroupWaitBits(
                suspendAckGroup, waitBits,
                pdTRUE, pdTRUE,
                pdMS_TO_TICKS(TASK_SUSPEND_TIMEOUT_MS));
            if ((got & waitBits) != waitBits)
                Serial.println("[SYSTEM] WARNING: climate task did not ack suspend in time");
        }

        // Resume incubator tasks — clear any stale TASK_CMD_SUSPEND notification
        // before resuming so they do not immediately re-suspend themselves
        if (hTaskTempControl != nullptr) {
            xTaskNotifyStateClear(hTaskTempControl);
            vTaskResume(hTaskTempControl);
        }
        if (hTaskTurner    != nullptr) { xTaskNotifyStateClear(hTaskTurner);    vTaskResume(hTaskTurner);    }
        if (hTaskFan       != nullptr) { xTaskNotifyStateClear(hTaskFan);       vTaskResume(hTaskFan);       }
        if (hTaskPump      != nullptr) { xTaskNotifyStateClear(hTaskPump);      vTaskResume(hTaskPump);      }
        if (hTaskMilestone != nullptr) { xTaskNotifyStateClear(hTaskMilestone); vTaskResume(hTaskMilestone); }

        Serial.println("[PROFILE] Switched to Egg Incubator");

    } else {
        // Build wait mask — skip tasks already suspended (e.g. turner at lockdown)
        EventBits_t waitBits = 0;
        xEventGroupClearBits(suspendAckGroup, TASK_SUSPEND_BITS_INCUBATOR);

        if (hTaskTempControl != nullptr &&
            eTaskGetState(hTaskTempControl) != eSuspended) {
            xTaskNotify(hTaskTempControl, TASK_CMD_SUSPEND, eSetValueWithOverwrite);
            waitBits |= TASK_SUSPEND_BIT_TEMP_CTRL;
        }
        if (hTaskTurner != nullptr &&
            eTaskGetState(hTaskTurner) != eSuspended) {
            xTaskNotify(hTaskTurner, TASK_CMD_SUSPEND, eSetValueWithOverwrite);
            waitBits |= TASK_SUSPEND_BIT_TURNER;
        }
        if (hTaskFan != nullptr &&
            eTaskGetState(hTaskFan) != eSuspended) {
            xTaskNotify(hTaskFan, TASK_CMD_SUSPEND, eSetValueWithOverwrite);
            waitBits |= TASK_SUSPEND_BIT_FAN;
        }
        if (hTaskPump != nullptr &&
            eTaskGetState(hTaskPump) != eSuspended) {
            xTaskNotify(hTaskPump, TASK_CMD_SUSPEND, eSetValueWithOverwrite);
            waitBits |= TASK_SUSPEND_BIT_PUMP;
        }
        if (hTaskMilestone != nullptr &&
            eTaskGetState(hTaskMilestone) != eSuspended) {
            xTaskNotify(hTaskMilestone, TASK_CMD_SUSPEND, eSetValueWithOverwrite);
            waitBits |= TASK_SUSPEND_BIT_MILESTONE;
        }

        if (waitBits) {
            EventBits_t got = xEventGroupWaitBits(
                suspendAckGroup, waitBits,
                pdTRUE, pdTRUE,
                pdMS_TO_TICKS(TASK_SUSPEND_TIMEOUT_MS));
            if ((got & waitBits) != waitBits)
                Serial.println("[SYSTEM] WARNING: not all incubator tasks acked suspend in time");
        }

        // Resume climate task — clear any stale notification first
        if (hTaskClimateControl != nullptr) {
            xTaskNotifyStateClear(hTaskClimateControl);
            vTaskResume(hTaskClimateControl);
        }

        Serial.println("[PROFILE] Switched to Climate Chamber");
    }

    saveSettings();
}

// ─────────────────────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[SETUP] Boot v" FW_VERSION);

    // ── Relay pins — drive OFF immediately before anything else ──────────────
    pinMode(RELAY_HEATER,     OUTPUT); digitalWrite(RELAY_HEATER,     RELAY_OFF);
    pinMode(RELAY_COOLER,     OUTPUT); digitalWrite(RELAY_COOLER,     RELAY_OFF);
    pinMode(RELAY_HUMIDIFIER, OUTPUT); digitalWrite(RELAY_HUMIDIFIER, RELAY_OFF);
    pinMode(RELAY_FAN,        OUTPUT); digitalWrite(RELAY_FAN,        RELAY_OFF);
    pinMode(RELAY_PUMP,       OUTPUT); digitalWrite(RELAY_PUMP,       RELAY_OFF);
    pinMode(RELAY_TURNER,     OUTPUT); digitalWrite(RELAY_TURNER,     RELAY_OFF);

    // ── Fan PWM — init LEDC once, before any task can call setFanSpeed() ─────
    initFanPwm();

    // ── Button pins ──────────────────────────────────────────────────────────
    pinMode(BTN_UP,   INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_OK,   INPUT_PULLUP);

    // ── I2C bus ──────────────────────────────────────────────────────────────
    Wire.begin(I2C_SDA, I2C_SCL);

    // ── RTC ──────────────────────────────────────────────────────────────────
    // A missing or failed RTC is not fatal: temperature/humidity control works
    // without wall-clock time. Epoch-dependent logic (turner, milestones, cyclic
    // phase) is already gated on rtcEpochValid, so it stays dormant until the
    // clock is set via the UI or NTP.
    bool clockSetFromCompileTime = false;
    if (!rtc.begin()) {
        Serial.println("[SETUP] RTC not found — continuing without RTC");
        rtcEpochValid = false;
    } else if (!rtc.isrunning()) {
        Serial.println("[SETUP] RTC not running — setting to compile time");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        clockSetFromCompileTime = true;
    }

    // ── RTOS primitives ──────────────────────────────────────────────────────
    sensorMutex   = xSemaphoreCreateMutex();
    rtcMutex      = xSemaphoreCreateMutex();
    controlMutex  = xSemaphoreCreateMutex();
    settingsMutex = xSemaphoreCreateMutex();
    milestoneMutex = xSemaphoreCreateMutex();
    uiEventQueue  = xQueueCreate(UI_EVENT_QUEUE_SIZE,  sizeof(UiEvent));
    errorQueue    = xQueueCreate(ERROR_QUEUE_SIZE, sizeof(ErrorMsg_t));
    telemetryQueue  = xQueueCreate(TELEMETRY_QUEUE_SIZE, sizeof(TelemetryMsg_t));
    suspendAckGroup = xEventGroupCreate();

    // Compile time passes the epoch sanity gate but is NOT the real time —
    // warn the operator so they verify/set the clock (BUG-028).
    if (clockSetFromCompileTime) {
        pushError("CLOCK_UNSET", "RTC set from compile time — verify clock");
    }

    // ── Load settings from NVS ───────────────────────────────────────────────
    loadSettings();

    // ── Apply manual relay state if in manual mode at boot ──────────────────
    {
        ControlMode  cm = MODE_AUTO;
        bool         hm = false, clm = false;
        ProfileType  p  = PROFILE_EGG_INCUBATOR;
        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            cm  = gSettings.controlMode;
            hm  = gSettings.heaterManualOn;
            clm = gSettings.coolerManualOn;
            p   = gSettings.activeProfile;
            xSemaphoreGive(settingsMutex);
        }
        if (cm == MODE_MANUAL) {
            setRelay(RELAY_HEATER, hm);
            if (p == PROFILE_CLIMATE_CHAMBER) setRelay(RELAY_COOLER, clm);
        }
    }

    // ── WiFi (non-blocking — try stored credentials only, never blocks) ─────
    //   Portal is NOT opened automatically.  The user can start it manually
    //   via Settings → WiFi → Connect.  If no stored credentials exist,
    //   WiFi.begin() returns quickly and the device boots offline.
    WiFi.mode(WIFI_STA);
    WiFi.begin();   // attempt stored SSID/password; returns immediately
    Serial.println("[SETUP] WiFi.begin() called (non-blocking)");

    // ── Create all tasks ─────────────────────────────────────────────────────
    //                              function                 name           stack   param  prio  handle   core
    xTaskCreatePinnedToCore(task_rtc,                    "RTC",          3072, nullptr, 2, nullptr,           1);
    xTaskCreatePinnedToCore(task_buttons,                "Buttons",      2048, nullptr, 3, nullptr,           1);
    xTaskCreatePinnedToCore(task_ui,                     "UI",           6144, nullptr, 2, nullptr,           1);
    xTaskCreatePinnedToCore(task_sensor,                 "DHT",          3072, nullptr, 3, nullptr,           1);
    xTaskCreatePinnedToCore(task_ds18b20,                "DS18B20",      4096, nullptr, 3, nullptr,           1);

    // Control tasks — stored handles for profile switching
    xTaskCreatePinnedToCore(task_temperature_control,    "TempCtrl",     4096, nullptr, 4, &hTaskTempControl,   1);
    xTaskCreatePinnedToCore(task_climate_control,        "ClimCtrl",     4096, nullptr, 4, &hTaskClimateControl,1);

    // Incubator-specific tasks
    xTaskCreatePinnedToCore(task_turner,                 "Turner",       2048, nullptr, 2, &hTaskTurner,        1);
    xTaskCreatePinnedToCore(task_fan,                    "Fan",          2048, nullptr, 2, &hTaskFan,           1);
    xTaskCreatePinnedToCore(task_pump,                   "Pump",         2048, nullptr, 2, &hTaskPump,          1);
    xTaskCreatePinnedToCore(task_milestone,              "Milestone",    2048, nullptr, 1, &hTaskMilestone,     1);

    // Cloud on Core 0
    xTaskCreatePinnedToCore(task_cloud,                  "Cloud",       12288, nullptr, 1, nullptr,            0);

    // MQTT on Core 0 — additive alongside Cloud, publishes the same
    // telemetry to a local broker; independent of the Google Sheets path.
    xTaskCreatePinnedToCore(task_mqtt,                   "MQTT",         6144, nullptr, 1, nullptr,            0);

    // WiFi manager on Core 0 (lowest priority — never blocks other tasks)
    xTaskCreatePinnedToCore(task_wifi_manager,           "WifiMgr",     8192,  nullptr, 1, nullptr,            0);

    // ── Apply profile: suspend tasks that don't belong to active profile ─────
    ProfileType p = PROFILE_EGG_INCUBATOR;
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        p = gSettings.activeProfile;
        xSemaphoreGive(settingsMutex);
    }

    // Small delay to let tasks initialise before suspend
    vTaskDelay(pdMS_TO_TICKS(100));

    if (p == PROFILE_EGG_INCUBATOR) {
        if (hTaskClimateControl != nullptr) vTaskSuspend(hTaskClimateControl);
    } else {
        if (hTaskTempControl != nullptr)    vTaskSuspend(hTaskTempControl);
        if (hTaskTurner      != nullptr)    vTaskSuspend(hTaskTurner);
        if (hTaskFan         != nullptr)    vTaskSuspend(hTaskFan);
        if (hTaskPump        != nullptr)    vTaskSuspend(hTaskPump);
        if (hTaskMilestone   != nullptr)    vTaskSuspend(hTaskMilestone);
    }

    Serial.println("[SETUP] All tasks started");

    // ── Task watchdog ────────────────────────────────────────────────────────
    // Subscribed tasks: the ACTIVE profile's control task (500 ms loop),
    // DHT (≤ 3 500 ms), DS18B20 (≤ 1 800 ms). 10 s gives ≈ 2.8× margin.
    // Long-sleep tasks (turner, pump, cloud) are intentionally NOT subscribed.
    //
    // Control-task subscription is done here — AFTER the initial vTaskSuspend
    // calls — so the inactive profile's task is never subscribed while frozen.
    // Sensor tasks self-subscribe in their own preambles; they are never
    // suspended by setup(), so that is safe.
    //
    // Profile-switch paths in the control tasks handle subscribe/unsubscribe
    // dynamically: esp_task_wdt_delete(NULL) before vTaskSuspend(NULL) and
    // esp_task_wdt_add(NULL) immediately after.
    //
    // On Arduino-ESP32 core ≥ 3.x (IDF v5) the TWDT is already initialised by
    // the framework; esp_task_wdt_init() returns ESP_ERR_INVALID_STATE in that
    // case — use esp_task_wdt_reconfigure() instead.
    const esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms    = 10000,   // 10 s
        .idle_core_mask = 0,      // do not watch idle tasks
        .trigger_panic  = true
    };
    {
        esp_err_t wdt_err = esp_task_wdt_init(&wdt_cfg);
        if (wdt_err == ESP_ERR_INVALID_STATE) {
            // Already initialised by the Arduino framework — just reconfigure.
            wdt_err = esp_task_wdt_reconfigure(&wdt_cfg);
        }
        if (wdt_err != ESP_OK) {
            Serial.printf("[WDT] Init/reconfigure failed: %s\n", esp_err_to_name(wdt_err));
        } else {
            Serial.println("[WDT] Initialised (10 s, panic on expire)");
        }
    }

    // Subscribe the active profile's control task now that it is running and
    // the inactive one has already been suspended.
    if (p == PROFILE_EGG_INCUBATOR) {
        if (hTaskTempControl != nullptr)    esp_task_wdt_add(hTaskTempControl);
    } else {
        if (hTaskClimateControl != nullptr) esp_task_wdt_add(hTaskClimateControl);
    }
    Serial.println("[WDT] Control task subscribed");
}

// ─────────────────────────────────────────────────────────────────────────────
// LOOP — empty, all work done in tasks
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    vTaskDelay(portMAX_DELAY);
}
