#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>

#include "RTClib.h"

#include "config.h"
#include "globals.h"
#include "task_sensors.h"
#include "task_control.h"
#include "task_rtc.h"
#include "task_buttons.h"
#include "task_ui.h"
#include "task_cloud.h"
#include "task_incubator.h"
#include "task_wifi_manager.h"

// Explicit C++ prototypes — required because these .cpp files include Arduino.h
// after their own header, causing C-linkage/C++-linkage mismatch with this TU.
void task_rtc(void* pvParameters);
void task_cloud(void* pvParameters);
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
        gSettings.fanIntervalMin      = prefs.getUInt("fanIntv",    DEFAULT_FAN_INTERVAL_MIN);
        gSettings.fanDurationSec      = prefs.getUInt("fanDur",     DEFAULT_FAN_DURATION_SEC);
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

        xSemaphoreGive(settingsMutex);
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
    prefs.putUInt("fanIntv",    snap.fanIntervalMin);
    prefs.putUInt("fanDur",     snap.fanDurationSec);
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
    prefs.begin("incubator", false);
    prefs.clear();
    prefs.end();
    Serial.println("[NVS] Factory reset — restarting");
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP.restart();
}

// ─────────────────────────────────────────────────────────────────────────────
// switchProfile — suspends/resumes appropriate tasks, resets relays
// ─────────────────────────────────────────────────────────────────────────────
void switchProfile(ProfileType newProfile) {
    allRelaysOff();

    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        gSettings.activeProfile = newProfile;
        xSemaphoreGive(settingsMutex);
    }

    if (newProfile == PROFILE_EGG_INCUBATOR) {
        // Activate incubator tasks
        if (hTaskTempControl    != nullptr) vTaskResume(hTaskTempControl);
        if (hTaskTurner         != nullptr) vTaskResume(hTaskTurner);
        if (hTaskFan            != nullptr) vTaskResume(hTaskFan);
        if (hTaskPump           != nullptr) vTaskResume(hTaskPump);
        if (hTaskMilestone      != nullptr) vTaskResume(hTaskMilestone);
        // Suspend climate task
        if (hTaskClimateControl != nullptr) vTaskSuspend(hTaskClimateControl);

        Serial.println("[PROFILE] Switched to Egg Incubator");

    } else {
        // Activate climate task
        if (hTaskClimateControl != nullptr) vTaskResume(hTaskClimateControl);
        // Suspend incubator-only tasks
        if (hTaskTempControl    != nullptr) vTaskSuspend(hTaskTempControl);
        if (hTaskTurner         != nullptr) vTaskSuspend(hTaskTurner);
        if (hTaskFan            != nullptr) vTaskSuspend(hTaskFan);
        if (hTaskPump           != nullptr) vTaskSuspend(hTaskPump);
        if (hTaskMilestone      != nullptr) vTaskSuspend(hTaskMilestone);

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

    // ── Button pins ──────────────────────────────────────────────────────────
    pinMode(BTN_UP,   INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_OK,   INPUT_PULLUP);

    // ── I2C bus ──────────────────────────────────────────────────────────────
    Wire.begin(I2C_SDA, I2C_SCL);

    // ── RTC ──────────────────────────────────────────────────────────────────
    if (!rtc.begin()) {
        Serial.println("[SETUP] RTC not found — halting");
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    if (!rtc.isrunning()) {
        Serial.println("[SETUP] RTC not running — setting to compile time");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // ── RTOS primitives ──────────────────────────────────────────────────────
    sensorMutex   = xSemaphoreCreateMutex();
    rtcMutex      = xSemaphoreCreateMutex();
    controlMutex  = xSemaphoreCreateMutex();
    settingsMutex = xSemaphoreCreateMutex();
    uiEventQueue  = xQueueCreate(UI_EVENT_QUEUE_SIZE,  sizeof(UiEvent));
    errorQueue    = xQueueCreate(ERROR_QUEUE_SIZE, sizeof(ErrorMsg_t));

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
}

// ─────────────────────────────────────────────────────────────────────────────
// LOOP — empty, all work done in tasks
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    vTaskDelay(portMAX_DELAY);
}
