#include "task_ui.h"
#include "globals.h"
#include "config.h"
#include "oled_ui.h"
#include "incubator_logic.h"
#include "task_incubator.h"
#include "task_wifi_manager.h"
#include <WiFi.h>
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

extern RTC_DS1307 rtc;  // defined in egg_incubator_v2.ino
 
// Helper: stringify UI events/states for logging
static const char* uiEventName(UiEvent e) {
    switch (e) {
        case UI_EVT_NONE: return "NONE";
        case UI_EVT_UP:   return "UP";
        case UI_EVT_DOWN: return "DOWN";
        case UI_EVT_OK:   return "OK";
        default: return "?";
    }
}

static const char* uiStateName(UiState s) {
    switch (s) {
        case UI_HOME: return "HOME";
        case UI_MAIN_MENU: return "MAIN_MENU";
        case UI_EGG_INCUBATOR_MENU: return "EGG_INC_MENU";
        case UI_CLIMATE_CHAMBER_MENU: return "CLIM_MENU";
        case UI_SYSTEM_MENU: return "SYSTEM_MENU";
        case UI_PUMP_SETTINGS: return "PUMP_SETTINGS";
        case UI_CONTROLLER_MODE_MENU: return "CONTROLLER_MODE_MENU";
        case UI_ENV_TEMPERATURE: return "ENV_TEMPERATURE";
        case UI_ENV_HUMIDITY: return "ENV_HUMIDITY";
        case UI_ENV_INCUBATION_DAY: return "ENV_INCUBATION_DAY";
        case UI_ENV_EGG_TYPE: return "ENV_EGG_TYPE";
        case UI_ENV_TURNER: return "ENV_TURNER";
        default: return "OTHER";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Forward declarations for NVS functions (defined in main .ino)
// ─────────────────────────────────────────────────────────────────────────────
extern void saveSettings(void);
extern void switchProfile(ProfileType newProfile);

// ─────────────────────────────────────────────────────────────────────────────
// UI state and navigation indices — local to this task only
// ─────────────────────────────────────────────────────────────────────────────
static UiState  uiState          = UI_HOME;
static int      mainMenuIdx      = 0;
static int      controllerModeIdx= 0;
static int      modeMenuIdx      = 0;
static int      manualCtrlIdx    = 0;
static int      hysteresisMenuIdx= 0;
static int      eggTypeIdx       = 0;
static int      turnerMenuIdx    = 0;
static int      fanMenuIdx       = 0;
static int      climateModeIdx   = 0;
static int      wifiMenuIdx      = 0;  // WiFi menu: 0=Connect/Disconnect, 1=Back
static int      climateSchedIdx  = 0;
static int      climateCyclicIdx = 0;
static int      climateRampIdx   = 0;
static int      eggMenuIdx       = 0;
static int      eggMenuTop       = 0;
static int      climMenuIdx      = 0;
static int      climMenuTop      = 0;
static int      sysMenuIdx       = 0;
static int      sysMenuTop       = 0;
static int      pumpMenuIdx      = 0;
static UiState  profileMenuParent = UI_EGG_INCUBATOR_MENU;
static uint16_t editPumpDur      = DEFAULT_PUMP_DURATION_SEC;
static bool     pumpEditing      = false;
static int      lastMenuIdx      = -1;

// Date editing state for incubation start
static int editDay   = 1;
static int editMonth = 1;
static int editYear  = 2024;
static int editField = 0;  // 0=day, 1=month, 2=year

// Time & Date RTC editor state
static int  timeDateMenuIdx = 0;     // index in Time & Date sub-menu (0,1,2)
static int  tEditField      = 0;     // 0=H,1=M,2=S,3=D,4=Mo,5=Y,6=SAVE?
static int  tH = 12, tM = 0, tS = 0; // edit buffer – time
static int  tD = 1,  tMo = 1, tY = 2024; // edit buffer – date

// Home screen refresh tracking
static int           lastMinute         = -1;
static unsigned long lastSensorUiMs     = 0;
static unsigned long lastTempUiRefreshMs= 0;
static unsigned long lastOkUiMs = 0;

// ─────────────────────────────────────────────────────────────────────────────
// Helper: read a snapshot of all display data under their respective mutexes
// ─────────────────────────────────────────────────────────────────────────────
static void readDisplayData(float& temp, float& hum, DateTime& now,
                             float& tempSP, float& humSP,
                             bool& isAuto, RelayState_t& rs,
                             ProfileType& profile)
{
    if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
        temp = gSensorData.temp_ds18b20;
        hum  = gSensorData.humidity_dht;
        xSemaphoreGive(sensorMutex);
    }
    if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
        now = gRtcTime.now;
        xSemaphoreGive(rtcMutex);
    }
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
        tempSP  = gSettings.tempSetpoint;
        humSP   = gSettings.humSetpoint;
        isAuto  = (gSettings.controlMode == MODE_AUTO);
        profile = gSettings.activeProfile;
        xSemaphoreGive(settingsMutex);
    }
    if (xSemaphoreTake(controlMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
        rs = gRelayState;
        xSemaphoreGive(controlMutex);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: read current setpoints
// ─────────────────────────────────────────────────────────────────────────────
static void readSettings(float& tempSP, float& tempHyst,
                          float& humSP,  float& humHyst,
                          ProfileType& profile,
                          ControlMode& mode,
                          ClimateModeType& climMode)
{
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        tempSP   = gSettings.tempSetpoint;
        tempHyst = gSettings.tempHysteresis;
        humSP    = gSettings.humSetpoint;
        humHyst  = gSettings.humHysteresis;
        profile  = gSettings.activeProfile;
        mode     = gSettings.controlMode;
        climMode = gSettings.climateMode;
        xSemaphoreGive(settingsMutex);
    }
}

// Returns the number of days in a given month (1-12) for a given year.
static uint8_t daysInMonth(int m, int y) {
    if (m == 2) {
        bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
        return leap ? 29 : 28;
    }
    static const uint8_t dom[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    return dom[m];
}

// Factory reset confirm selection: 0 = No (default), 1 = Yes (BUG-034)
static int factoryResetIdx = 0;

// Climate schedule / cyclic edit buffers — committed to gSettings only on
// final OK so the control task never sees half-edited values (BUG-035)
static uint8_t  editSchedStart = DEFAULT_SCHED_START_HOUR;
static uint8_t  editSchedEnd   = DEFAULT_SCHED_END_HOUR;
static uint32_t editHeatPeriod = DEFAULT_CLIMATE_HEAT_PERIOD_MIN;
static uint32_t editCoolPeriod = DEFAULT_CLIMATE_COOL_PERIOD_MIN;

// NTP sync progress tracking for the non-blocking UI_TIME_WIFI_SYNC flow (BUG-033)
static unsigned long ntpSyncStartMs   = 0;
static unsigned long ntpResultShownMs = 0;
static int           ntpSyncResult    = 0;   // 0 = pending

// BUG-036: a failed settingsMutex take in an editor silently discards the
// user's button press — make the (rare) event visible for diagnosis.
static void logSettingsMutexTimeout(void) {
    Serial.println("[UI] settingsMutex timeout — adjustment dropped");
}

// Discard any queued button events (e.g. presses made during an NTP sync)
static void drainUiQueue(void) {
    UiEvent dummy;
    while (xQueueReceive(uiEventQueue, &dummy, 0) == pdTRUE) {}
}

// Drive the NTP sync screen forward; called on idle ticks and on events while
// in UI_TIME_WIFI_SYNC. Shows the result when available (or after a 6 s
// timeout) and auto-returns to the System menu 1.5 s later.
static void ntpSyncPoll(void) {
    if (ntpSyncResult == 0) {
        int r = wifi_get_ntp_result();
        if (r == 0 && (millis() - ntpSyncStartMs) > 6000) r = 2;  // timeout
        if (r != 0) {
            ntpSyncResult = r;
            oled_show_time_wifi_sync(r);
            ntpResultShownMs = millis();
        }
    } else if (millis() - ntpResultShownMs > 1500) {
        drainUiQueue();   // presses made during the sync must not replay
        uiState = UI_SYSTEM_MENU;
        oled_show_system_menu(sysMenuIdx, sysMenuTop);
        lastMenuIdx = sysMenuIdx;
    }
}

// Incubation start-date screen state (UI_ENV_INCUBATION_DAY)
enum IncubMode { IM_NAV = 0, IM_EDIT };
static IncubMode incubMode   = IM_NAV;
static int       incubNavIdx = 0;   // 0=Start Date, 1=Back
static int       dispDay = 1, dispMonth = 1, dispYear = 2024;

// Load the saved (or current RTC) date snapshot and draw the navigation view.
// Called directly from the Egg menu OK handler so the screen appears on the
// first press (BUG-038 — the state used to be entered without drawing, so the
// next press ran the init instead of acting: double-press to enter).
static void enterIncubationScreen(void) {
    uint32_t sEpoch = 0;
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        sEpoch = gSettings.startEpoch;
        xSemaphoreGive(settingsMutex);
    }
    if (sEpoch != 0) {
        DateTime sd((uint32_t)sEpoch);
        dispDay = sd.day(); dispMonth = sd.month(); dispYear = sd.year();
    } else {
        DateTime now(2024, 1, 1);
        if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            now = gRtcTime.now;
            xSemaphoreGive(rtcMutex);
        }
        dispDay = now.day(); dispMonth = now.month(); dispYear = now.year();
    }
    incubMode = IM_NAV; incubNavIdx = 0; editField = 0;
    oled_show_incubation_day_set(incubNavIdx, dispDay, dispMonth, dispYear, false, 0);
    lastMenuIdx = incubNavIdx;
}

// Periodic redraw of the live sensor value on the temp/hum edit screens while
// no buttons are pressed (BUG-032 — handlers below only run on events).
static void uiIdleRefresh(void) {
    if (uiState != UI_ENV_TEMPERATURE && uiState != UI_ENV_HUMIDITY) return;
    if (millis() - lastTempUiRefreshMs < 1000) return;
    lastTempUiRefreshMs = millis();

    float cur = 0.0f, sp = 0.0f;
    if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
        cur = (uiState == UI_ENV_TEMPERATURE) ? gSensorData.temp_ds18b20
                                              : gSensorData.humidity_dht;
        xSemaphoreGive(sensorMutex);
    }
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
        sp = (uiState == UI_ENV_TEMPERATURE) ? gSettings.tempSetpoint
                                             : gSettings.humSetpoint;
        xSemaphoreGive(settingsMutex);
    }
    if (uiState == UI_ENV_TEMPERATURE) oled_show_temperature(cur, sp);
    else                               oled_show_humidity(cur, sp);
}

// ─────────────────────────────────────────────────────────────────────────────
// TASK: UI STATE MACHINE
// ─────────────────────────────────────────────────────────────────────────────
void task_ui(void* pvParameters) {
    if (!oled_init()) {
        pushError("FAULT", "OLED init failed — UI disabled");
        // Idle rather than busy-spin; control tasks on the other core keep running.
        for (;;) { vTaskDelay(pdMS_TO_TICKS(5000)); }
    }

    for (;;) {

        // ── FAULT MODE overrides everything ─────────────────────────────────
        bool fault = false;
        portENTER_CRITICAL(&faultMux);
        fault = overTempFault;
        portEXIT_CRITICAL(&faultMux);

        if (fault) {
            float faultTemp = 0.0f;
            if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                faultTemp = gSensorData.temp_ds18b20;
                xSemaphoreGive(sensorMutex);
            }
            oled_show_fault_screen(faultTemp);
            // Clear event queue so stale events don't fire after reset
            UiEvent dummy;
            while (xQueueReceive(uiEventQueue, &dummy, 0) == pdTRUE) {}
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // ── HOME SCREEN periodic refresh ─────────────────────────────────────
        if (uiState == UI_HOME) {
            float       temp = 0.0f, hum = 0.0f, tempSP = 0.0f, humSP = 0.0f;
            DateTime    now;
            bool        isAuto = true;
            RelayState_t rs = {};
            ProfileType profile = PROFILE_EGG_INCUBATOR;

            readDisplayData(temp, hum, now, tempSP, humSP, isAuto, rs, profile);

            bool redraw = false;
            if (now.minute() != lastMinute) { lastMinute = now.minute(); redraw = true; }
            if (millis() - lastSensorUiMs > 3000) { lastSensorUiMs = millis(); redraw = true; }

            if (redraw) {
                bool wifiOk = (WiFi.status() == WL_CONNECTED);
                if (profile == PROFILE_EGG_INCUBATOR) {
                    int day = calcIncubationDay();
                    int left = calcDaysLeft();
                    char milestone[24] = { 0 };
                    if (milestoneMutex && xSemaphoreTake(milestoneMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                        strncpy(milestone, gMilestoneLabel, sizeof(milestone) - 1);
                        xSemaphoreGive(milestoneMutex);
                    }
                    oled_show_home_incubator(now, day, left, temp, hum,
                                             tempSP, humSP, isAuto,
                                             rs.heaterOn, rs.humidifierOn,
                                             rs.fanOn, rs.turnerOn,
                                             wifiOk, milestone);
                } else {
                    char phase[12] = "--";
                    ClimateModeType climMode = CLIMATE_FIXED_SCHEDULE;
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                        climMode = gSettings.climateMode;
                        xSemaphoreGive(settingsMutex);
                    }
                    if (climMode == CLIMATE_FIXED_SCHEDULE) {
                        if      (rs.heaterOn) snprintf(phase, sizeof(phase), "HEAT");
                        else if (rs.coolerOn) snprintf(phase, sizeof(phase), "COOL");
                        else                  snprintf(phase, sizeof(phase), "IDLE");
                    } else if (climMode == CLIMATE_CYCLIC) {
                        if      (rs.heaterOn) snprintf(phase, sizeof(phase), "HEAT");
                        else if (rs.coolerOn) snprintf(phase, sizeof(phase), "COOL");
                        else                  snprintf(phase, sizeof(phase), "IDLE");
                    } else {
                        uint8_t si = 0;
                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                            si = gSettings.rampStepIdx;
                            xSemaphoreGive(settingsMutex);
                        }
                        snprintf(phase, sizeof(phase), "RAMP %d", si + 1);
                    }
                    oled_show_home_climate(now, temp, hum, tempSP, humSP,
                                           isAuto, rs.heaterOn, rs.coolerOn,
                                           rs.humidifierOn, wifiOk, phase);
                }
            }
        }

        // ── EVENT PROCESSING ─────────────────────────────────────────────────
        UiEvent evt;
        if (xQueueReceive(uiEventQueue, &evt, pdMS_TO_TICKS(50)) != pdTRUE) {
            // No event this tick — run time-driven UI work (BUG-032 / BUG-033)
            if (uiState == UI_TIME_WIFI_SYNC) ntpSyncPoll();
            else                              uiIdleRefresh();
            continue;
        }

        // Verbose UI event logging to help debug missed OK/enter issues
        Serial.printf("[UI] Event received: %s  State: %s  lastMenuIdx=%d  ms=%lu\n",
                      uiEventName(evt), uiStateName(uiState), lastMenuIdx, millis());

        // Read current settings snapshot for use in any state below
        float tempSP = DEFAULT_TEMP_SETPOINT, tempHyst = DEFAULT_TEMP_HYSTERESIS;
        float humSP  = DEFAULT_HUM_SETPOINT,  humHyst  = DEFAULT_HUM_HYSTERESIS;
        ProfileType  profile  = PROFILE_EGG_INCUBATOR;
        ControlMode  ctrlMode = MODE_AUTO;
        ClimateModeType climMode = CLIMATE_FIXED_SCHEDULE;
        readSettings(tempSP, tempHyst, humSP, humHyst, profile, ctrlMode, climMode);

        // Rate-limit OK events to avoid overscrolling from repeated presses
        if (evt == UI_EVT_OK) {
            if ((millis() - lastOkUiMs) < 300) {
                continue; // drop this OK as it's too soon after the previous
            }
            lastOkUiMs = millis();
        }

        // ═════════════════════════════════════════════════════════════════════
        // HOME
        // ═════════════════════════════════════════════════════════════════════
        if (uiState == UI_HOME) {
            if (evt == UI_EVT_OK) {
                uiState = UI_MAIN_MENU;
                mainMenuIdx = 0; lastMenuIdx = -1;
                oled_show_menu(mainMenuIdx);
                    Serial.printf("[UI] Enter MAIN_MENU (from HOME) mainMenuIdx=%d\n", mainMenuIdx);
                lastMenuIdx = mainMenuIdx;
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // MAIN MENU
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_MAIN_MENU) {
            if      (evt == UI_EVT_UP)   mainMenuIdx = (mainMenuIdx - 1 + MENU_COUNT) % MENU_COUNT;
            else if (evt == UI_EVT_DOWN) mainMenuIdx = (mainMenuIdx + 1) % MENU_COUNT;

            if (mainMenuIdx != lastMenuIdx) {
                oled_show_menu(mainMenuIdx);
                lastMenuIdx = mainMenuIdx;
            }

            if (evt == UI_EVT_OK) {
                switch ((MainMenuItem)mainMenuIdx) {
                    case MENU_EGG_INCUBATOR:
                        uiState = UI_EGG_INCUBATOR_MENU;
                        eggMenuIdx = 0; eggMenuTop = 0; lastMenuIdx = -1;
                        oled_show_egg_incubator_menu(eggMenuIdx, eggMenuTop);
                        lastMenuIdx = eggMenuIdx;
                        break;
                    case MENU_CLIMATE_CHAMBER:
                        uiState = UI_CLIMATE_CHAMBER_MENU;
                        climMenuIdx = 0; climMenuTop = 0; lastMenuIdx = -1;
                        oled_show_climate_chamber_menu(climMenuIdx, climMenuTop);
                        lastMenuIdx = climMenuIdx;
                        break;
                    case MENU_SYSTEM:
                        uiState = UI_SYSTEM_MENU;
                        sysMenuIdx = 0; sysMenuTop = 0; lastMenuIdx = -1;
                        oled_show_system_menu(sysMenuIdx, sysMenuTop);
                        lastMenuIdx = sysMenuIdx;
                        break;
                    case MENU_BACK:
                        uiState = UI_HOME;
                        lastMinute = -1;
                        break;
                    default: break;
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // CONTROLLER MODE
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_CONTROLLER_MODE_MENU) {
            // 0=Egg Incubator, 1=Climate Chamber, 2=Back
            if      (evt == UI_EVT_UP)   controllerModeIdx = (controllerModeIdx - 1 + 3) % 3;
            else if (evt == UI_EVT_DOWN) controllerModeIdx = (controllerModeIdx + 1) % 3;

            if (controllerModeIdx != lastMenuIdx) {
                oled_show_controller_mode(controllerModeIdx, profile);
                lastMenuIdx = controllerModeIdx;
            }

            if (evt == UI_EVT_OK) {
                if (controllerModeIdx == 0) {
                    switchProfile(PROFILE_EGG_INCUBATOR);
                    uiState = UI_HOME; lastMinute = -1;
                } else if (controllerModeIdx == 1) {
                    switchProfile(PROFILE_CLIMATE_CHAMBER);
                    uiState = UI_HOME; lastMinute = -1;
                } else {
                    uiState = UI_SYSTEM_MENU; lastMenuIdx = -1;
                    oled_show_system_menu(sysMenuIdx, sysMenuTop);
                    lastMenuIdx = sysMenuIdx;
                }
            }
        }

        // (BUG-025: dead UI_SET_ENV_MENU handler removed — no state ever entered it)

        // ═════════════════════════════════════════════════════════════════════
        // TEMPERATURE EDIT
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_ENV_TEMPERATURE) {
            bool redraw = false;
            if (evt == UI_EVT_UP) {
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    gSettings.tempSetpoint = round1(gSettings.tempSetpoint + TEMP_SETPOINT_STEP);
                    if (gSettings.tempSetpoint > TEMP_SETPOINT_MAX) gSettings.tempSetpoint = TEMP_SETPOINT_MAX;
                    tempSP = gSettings.tempSetpoint;
                    xSemaphoreGive(settingsMutex);
                } else logSettingsMutexTimeout();
                redraw = true;
            } else if (evt == UI_EVT_DOWN) {
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    gSettings.tempSetpoint = round1(gSettings.tempSetpoint - TEMP_SETPOINT_STEP);
                    if (gSettings.tempSetpoint < TEMP_SETPOINT_MIN) gSettings.tempSetpoint = TEMP_SETPOINT_MIN;
                    tempSP = gSettings.tempSetpoint;
                    xSemaphoreGive(settingsMutex);
                } else logSettingsMutexTimeout();
                redraw = true;
            }

            // Periodic live refresh of current temp
            if (millis() - lastTempUiRefreshMs > 1000) {
                lastTempUiRefreshMs = millis();
                redraw = true;
            }

            if (redraw) {
                float curTemp = 0.0f;
                if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    curTemp = gSensorData.temp_ds18b20;
                    xSemaphoreGive(sensorMutex);
                }
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    tempSP = gSettings.tempSetpoint;
                    xSemaphoreGive(settingsMutex);
                }
                oled_show_temperature(curTemp, tempSP);
            }

            if (evt == UI_EVT_OK) {
                saveSettings();
                if (profileMenuParent == UI_CLIMATE_CHAMBER_MENU) {
                    uiState = UI_CLIMATE_CHAMBER_MENU; lastMenuIdx = -1;
                    oled_show_climate_chamber_menu(climMenuIdx, climMenuTop);
                    lastMenuIdx = climMenuIdx;
                } else {
                    uiState = UI_EGG_INCUBATOR_MENU; lastMenuIdx = -1;
                    oled_show_egg_incubator_menu(eggMenuIdx, eggMenuTop);
                    lastMenuIdx = eggMenuIdx;
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // HUMIDITY EDIT
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_ENV_HUMIDITY) {
            bool redraw = false;
            if (evt == UI_EVT_UP) {
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    gSettings.humSetpoint += HUM_SETPOINT_STEP;
                    if (gSettings.humSetpoint > HUM_SETPOINT_MAX) gSettings.humSetpoint = HUM_SETPOINT_MAX;
                    humSP = gSettings.humSetpoint;
                    xSemaphoreGive(settingsMutex);
                } else logSettingsMutexTimeout();
                redraw = true;
            } else if (evt == UI_EVT_DOWN) {
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    gSettings.humSetpoint -= HUM_SETPOINT_STEP;
                    if (gSettings.humSetpoint < HUM_SETPOINT_MIN) gSettings.humSetpoint = HUM_SETPOINT_MIN;
                    humSP = gSettings.humSetpoint;
                    xSemaphoreGive(settingsMutex);
                } else logSettingsMutexTimeout();
                redraw = true;
            }

            if (redraw) {
                float curHum = 0.0f;
                if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    curHum = gSensorData.humidity_dht;
                    xSemaphoreGive(sensorMutex);
                }
                oled_show_humidity(curHum, humSP);
            }

            if (evt == UI_EVT_OK) {
                saveSettings();
                if (profileMenuParent == UI_CLIMATE_CHAMBER_MENU) {
                    uiState = UI_CLIMATE_CHAMBER_MENU; lastMenuIdx = -1;
                    oled_show_climate_chamber_menu(climMenuIdx, climMenuTop);
                    lastMenuIdx = climMenuIdx;
                } else {
                    uiState = UI_EGG_INCUBATOR_MENU; lastMenuIdx = -1;
                    oled_show_egg_incubator_menu(eggMenuIdx, eggMenuTop);
                    lastMenuIdx = eggMenuIdx;
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // HYSTERESIS MENU
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_ENV_HYSTERESIS_MENU) {
            if      (evt == UI_EVT_UP)   hysteresisMenuIdx = (hysteresisMenuIdx - 1 + 3) % 3;
            else if (evt == UI_EVT_DOWN) hysteresisMenuIdx = (hysteresisMenuIdx + 1) % 3;

            if (hysteresisMenuIdx != lastMenuIdx) {
                oled_show_hysteresis_menu(hysteresisMenuIdx, tempHyst, humHyst);
                lastMenuIdx = hysteresisMenuIdx;
            }

            if (evt == UI_EVT_OK) {
                if      (hysteresisMenuIdx == 0) { uiState = UI_ENV_HYST_TEMP_EDIT; }
                else if (hysteresisMenuIdx == 1) { uiState = UI_ENV_HYST_HUM_EDIT;  }
                else {
                    saveSettings();
                    if (profileMenuParent == UI_CLIMATE_CHAMBER_MENU) {
                        uiState = UI_CLIMATE_CHAMBER_MENU; lastMenuIdx = -1;
                        oled_show_climate_chamber_menu(climMenuIdx, climMenuTop);
                        lastMenuIdx = climMenuIdx;
                    } else {
                        uiState = UI_EGG_INCUBATOR_MENU; lastMenuIdx = -1;
                        oled_show_egg_incubator_menu(eggMenuIdx, eggMenuTop);
                        lastMenuIdx = eggMenuIdx;
                    }
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // HYSTERESIS TEMP EDIT
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_ENV_HYST_TEMP_EDIT) {
            if (evt == UI_EVT_UP) {
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    gSettings.tempHysteresis = round1(gSettings.tempHysteresis + 0.1f);
                    if (gSettings.tempHysteresis > TEMP_HYST_MAX) gSettings.tempHysteresis = TEMP_HYST_MAX;
                    tempHyst = gSettings.tempHysteresis;
                    xSemaphoreGive(settingsMutex);
                } else logSettingsMutexTimeout();
            } else if (evt == UI_EVT_DOWN) {
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    gSettings.tempHysteresis = round1(gSettings.tempHysteresis - 0.1f);
                    if (gSettings.tempHysteresis < TEMP_HYST_MIN) gSettings.tempHysteresis = TEMP_HYST_MIN;
                    tempHyst = gSettings.tempHysteresis;
                    xSemaphoreGive(settingsMutex);
                } else logSettingsMutexTimeout();
            }
            oled_show_hysteresis_menu(hysteresisMenuIdx, tempHyst, humHyst);
            if (evt == UI_EVT_OK) {
                saveSettings();
                uiState = UI_ENV_HYSTERESIS_MENU;
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // HYSTERESIS HUM EDIT
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_ENV_HYST_HUM_EDIT) {
            if (evt == UI_EVT_UP) {
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    gSettings.humHysteresis += 1.0f;
                    if (gSettings.humHysteresis > HUM_HYST_MAX) gSettings.humHysteresis = HUM_HYST_MAX;
                    humHyst = gSettings.humHysteresis;
                    xSemaphoreGive(settingsMutex);
                } else logSettingsMutexTimeout();
            } else if (evt == UI_EVT_DOWN) {
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    gSettings.humHysteresis -= 1.0f;
                    if (gSettings.humHysteresis < HUM_HYST_MIN) gSettings.humHysteresis = HUM_HYST_MIN;
                    humHyst = gSettings.humHysteresis;
                    xSemaphoreGive(settingsMutex);
                } else logSettingsMutexTimeout();
            }
            oled_show_hysteresis_menu(hysteresisMenuIdx, tempHyst, humHyst);
            if (evt == UI_EVT_OK) {
                saveSettings();
                uiState = UI_ENV_HYSTERESIS_MENU;
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // EGG TYPE SELECTION
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_ENV_EGG_TYPE) {
            // 0-3 = egg types, 4 = Back
            if      (evt == UI_EVT_UP)   eggTypeIdx = (eggTypeIdx - 1 + 5) % 5;
            else if (evt == UI_EVT_DOWN) eggTypeIdx = (eggTypeIdx + 1) % 5;

            if (eggTypeIdx != lastMenuIdx) {
                oled_show_egg_type(eggTypeIdx);
                lastMenuIdx = eggTypeIdx;
            }

            if (evt == UI_EVT_OK) {
                if (eggTypeIdx < 4) {
                    applyEggTypeDefaults((EggType)eggTypeIdx);
                    saveSettings();
                }
                uiState = UI_EGG_INCUBATOR_MENU; lastMenuIdx = -1;
                oled_show_egg_incubator_menu(eggMenuIdx, eggMenuTop);
                lastMenuIdx = eggMenuIdx;
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // INCUBATION START DATE (two-item menu: Start Date / Back)
        // Navigation mode vs Edit mode (separate)
        else if (uiState == UI_ENV_INCUBATION_DAY) {
            // Screen state lives at file scope; enterIncubationScreen() already
            // drew the navigation view when the Egg menu OK was handled, so
            // this is only a safety net for any other entry path (BUG-038).
            if (lastMenuIdx == -1) {
                enterIncubationScreen();
            }

            if (incubMode == IM_NAV) {
                // navigate between Start Date and Back
                if (evt == UI_EVT_UP)   incubNavIdx = (incubNavIdx - 1 + 2) % 2;
                else if (evt == UI_EVT_DOWN) incubNavIdx = (incubNavIdx + 1) % 2;

                // always redraw navigation row so a single UP/DOWN press updates immediately
                oled_show_incubation_day_set(incubNavIdx, dispDay, dispMonth, dispYear, false, 0);
                lastMenuIdx = incubNavIdx;

                if (evt == UI_EVT_OK) {
                    if (incubNavIdx == 0) {
                        // Enter edit mode: load edit values from saved or RTC
                        uint32_t sEpoch = 0;
                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            sEpoch = gSettings.startEpoch;
                            xSemaphoreGive(settingsMutex);
                        }
                        if (sEpoch != 0) {
                            DateTime sd((uint32_t)sEpoch);
                            editDay = sd.day(); editMonth = sd.month(); editYear = sd.year();
                        } else {
                            DateTime now(2024,1,1);
                            if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                                now = gRtcTime.now;
                                xSemaphoreGive(rtcMutex);
                            }
                            editDay = now.day(); editMonth = now.month(); editYear = now.year();
                        }
                        editField = 0;
                        incubMode = IM_EDIT;
                        oled_show_incubation_day_set(0, editDay, editMonth, editYear, true, editField);
                    } else {
                        // Back to Egg Incubator menu
                        uiState = UI_EGG_INCUBATOR_MENU; lastMenuIdx = -1;
                        oled_show_egg_incubator_menu(eggMenuIdx, eggMenuTop);
                        lastMenuIdx = eggMenuIdx;
                        continue;
                    }
                }
            } else { // IM_EDIT
                // UP/DOWN modify field
                if (evt == UI_EVT_UP) {
                    if (editField == 0) { editDay++;   if (editDay   > (int)daysInMonth(editMonth, editYear)) editDay = 1; }
                    else if (editField == 1) { editMonth++; if (editMonth > 12) editMonth = 1; if (editDay > (int)daysInMonth(editMonth, editYear)) editDay = daysInMonth(editMonth, editYear); }
                    else if (editField == 2) { editYear++;  if (editYear  > 2099) editYear = 2024; if (editDay > (int)daysInMonth(editMonth, editYear)) editDay = daysInMonth(editMonth, editYear); }
                    oled_show_incubation_day_set(0, editDay, editMonth, editYear, true, editField);
                } else if (evt == UI_EVT_DOWN) {
                    if (editField == 0) { editDay--;   if (editDay   < 1) editDay = daysInMonth(editMonth, editYear); }
                    else if (editField == 1) { editMonth--; if (editMonth < 1) editMonth = 12; if (editDay > (int)daysInMonth(editMonth, editYear)) editDay = daysInMonth(editMonth, editYear); }
                    else if (editField == 2) { editYear--;  if (editYear  < 2020) editYear = 2099; if (editDay > (int)daysInMonth(editMonth, editYear)) editDay = daysInMonth(editMonth, editYear); }
                    oled_show_incubation_day_set(0, editDay, editMonth, editYear, true, editField);
                } else if (evt == UI_EVT_OK) {
                    editField++;
                    if (editField < 3) {
                        oled_show_incubation_day_set(0, editDay, editMonth, editYear, true, editField);
                    } else {
                        // Final OK: clamp day then save startEpoch and reset lastTurnEpoch
                        if (editDay > (int)daysInMonth(editMonth, editYear)) editDay = daysInMonth(editMonth, editYear);
                        DateTime startDate(editYear, editMonth, editDay, 0, 0, 0);
                        uint32_t newStart = startDate.unixtime();

                        // Restore humidity to egg-type default (lockdown may have raised it).
                        float defaultHum = DEFAULT_HUM_SETPOINT;
                        EggType curEggType = EGG_CHICKEN;
                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            curEggType = gSettings.eggType;
                            xSemaphoreGive(settingsMutex);
                        }
                        switch (curEggType) {
                            case EGG_DUCK:  defaultHum = DUCK_DEFAULT_HUM;   break;
                            case EGG_QUAIL: defaultHum = QUAIL_DEFAULT_HUM;  break;
                            default:        defaultHum = CHICKEN_DEFAULT_HUM; break;
                        }

                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            gSettings.startEpoch    = newStart;
                            gSettings.lastTurnEpoch = 0;
                            gSettings.humSetpoint   = defaultHum;
                            xSemaphoreGive(settingsMutex);
                        }

                        // Resume the turner in case it was suspended for lockdown.
                        // vTaskResume is a no-op if the task is not suspended.
                        if (hTaskTurner != nullptr) {
                            vTaskResume(hTaskTurner);
                            Serial.println("[BATCH] Turner resumed for new batch");
                        }

                        // Persist only the startEpoch, lastTurn and setHum keys to NVS
                        {
                            Preferences prefs;
                            prefs.begin("incubator", false);
                            prefs.putULong("startEpoch", newStart);
                            prefs.putULong("lastTurn", 0);
                            prefs.putFloat("setHum", defaultHum);
                            prefs.end();
                        }

                        // Update navigation snapshot
                        uint32_t sEpoch = 0;
                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            sEpoch = gSettings.startEpoch;
                            xSemaphoreGive(settingsMutex);
                        }
                        if (sEpoch != 0) {
                            DateTime sd((uint32_t)sEpoch);
                            dispDay = sd.day(); dispMonth = sd.month(); dispYear = sd.year();
                        }

                        // Return to navigation mode
                        incubMode = IM_NAV; incubNavIdx = 0; lastMenuIdx = incubNavIdx;
                        oled_show_incubation_day_set(incubNavIdx, dispDay, dispMonth, dispYear, false, 0);
                    }
                }
            }
        }

        
        // ═════════════════════════════════════════════════════════════════════
        // TURNER SETTINGS (Egg incubator only)
        // Two-layer: navigation + edit/toggle modes
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_ENV_TURNER) {
            enum TurnerEditState { T_EDIT_NAV = 0, T_EDIT_INTERVAL, T_EDIT_DURATION, T_EDIT_TOGGLE };
            static TurnerEditState turnerEditState = T_EDIT_NAV;
            // persisted local edit state while in the screen
            static uint16_t editInterval = DEFAULT_TURNER_INTERVAL_MIN;
            static uint16_t editDuration = DEFAULT_TURNER_DURATION_SEC;
            static bool     editTurnNow  = false;

            // Initialize on first entry
            if (lastMenuIdx == -1) {
                turnerMenuIdx = 0;
                turnerEditState = T_EDIT_NAV;
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    editInterval = gSettings.turnerIntervalMin;
                    editDuration = gSettings.turnerDurationSec;
                    xSemaphoreGive(settingsMutex);
                }
                editTurnNow = false;
                oled_show_turner_settings(turnerMenuIdx, editInterval, editDuration, (int)turnerEditState, editTurnNow);
                lastMenuIdx = turnerMenuIdx;
                // No `continue` — fall through so the triggering event is
                // processed immediately (BUG-031; same fix as incubation screen).
            }

            // Navigation mode
            if (turnerEditState == T_EDIT_NAV) {
                if      (evt == UI_EVT_UP)   turnerMenuIdx = (turnerMenuIdx - 1 + 4) % 4;
                else if (evt == UI_EVT_DOWN) turnerMenuIdx = (turnerMenuIdx + 1) % 4;

                if (turnerMenuIdx != lastMenuIdx) {
                    oled_show_turner_settings(turnerMenuIdx, editInterval, editDuration, (int)turnerEditState, editTurnNow);
                    lastMenuIdx = turnerMenuIdx;
                }

                if (evt == UI_EVT_OK) {
                    switch (turnerMenuIdx) {
                        case 0: turnerEditState = T_EDIT_INTERVAL; break;
                        case 1: turnerEditState = T_EDIT_DURATION; break;
                        case 2: turnerEditState = T_EDIT_TOGGLE; editTurnNow = false; break;
                        case 3:
                            saveSettings();
                            uiState = UI_EGG_INCUBATOR_MENU; lastMenuIdx = -1;
                            oled_show_egg_incubator_menu(eggMenuIdx, eggMenuTop);
                            lastMenuIdx = eggMenuIdx;
                            break;
                    }
                    if (uiState == UI_ENV_TURNER) {
                        oled_show_turner_settings(turnerMenuIdx, editInterval, editDuration, (int)turnerEditState, editTurnNow);
                    } else {
                        continue; // we've switched out of this screen
                    }
                }
            }

            // Interval edit
            else if (turnerEditState == T_EDIT_INTERVAL) {
                if (evt == UI_EVT_UP)   editInterval = min((int)editInterval + 30, TURNER_MAX_INTERVAL_MIN);
                if (evt == UI_EVT_DOWN) editInterval = max((int)editInterval - 30, TURNER_MIN_INTERVAL_MIN);
                if (evt == UI_EVT_OK) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        gSettings.turnerIntervalMin = editInterval;
                        xSemaphoreGive(settingsMutex);
                    }
                    saveSettings();
                    turnerEditState = T_EDIT_NAV;
                }
                oled_show_turner_settings(turnerMenuIdx, editInterval, editDuration, (int)turnerEditState, editTurnNow);
            }

            // Duration edit
            else if (turnerEditState == T_EDIT_DURATION) {
                if (evt == UI_EVT_UP)   editDuration = min((int)editDuration + 5, TURNER_MAX_DURATION_SEC);
                if (evt == UI_EVT_DOWN) editDuration = max((int)editDuration - 5, TURNER_MIN_DURATION_SEC);
                if (evt == UI_EVT_OK) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        gSettings.turnerDurationSec = editDuration;
                        xSemaphoreGive(settingsMutex);
                    }
                    saveSettings();
                    turnerEditState = T_EDIT_NAV;
                }
                oled_show_turner_settings(turnerMenuIdx, editInterval, editDuration, (int)turnerEditState, editTurnNow);
            }

            // Toggle (Turn Now)
            else if (turnerEditState == T_EDIT_TOGGLE) {
                if (evt == UI_EVT_UP || evt == UI_EVT_DOWN) {
                    editTurnNow = !editTurnNow;
                }
                if (evt == UI_EVT_OK) {
                    if (editTurnNow) {
                        // Set lastTurnEpoch one full interval in the past so the turner
                        // fires on its next 10-s poll, rather than treating 0 as bootstrap.
                        uint32_t nowEp  = 0;
                        uint32_t intSec = 0;
                        if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            nowEp = gRtcTime.epoch;
                            xSemaphoreGive(rtcMutex);
                        }
                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            intSec = (uint32_t)gSettings.turnerIntervalMin * 60UL;
                            gSettings.lastTurnEpoch = (nowEp > intSec) ? (nowEp - intSec) : 0;
                            xSemaphoreGive(settingsMutex);
                        }
                        saveSettings();
                    }
                    turnerEditState = T_EDIT_NAV;
                }
                oled_show_turner_settings(turnerMenuIdx, editInterval, editDuration, (int)turnerEditState, editTurnNow);
            }
        }


        // ═════════════════════════════════════════════════════════════════════
        // FAN SETTINGS (0=Speed, 1=Back)
        // Two-layer: navigation + edit mode
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_ENV_FAN) {
            enum FanEditState { F_NAV = 0, F_EDIT_SPEED };
            static FanEditState fanEditState = F_NAV;
            static uint8_t editSpeed = DEFAULT_FAN_SPEED_PERCENT;

            // Initialize on entry
            if (lastMenuIdx == -1) {
                fanMenuIdx = 0;
                fanEditState = F_NAV;
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    editSpeed = gSettings.fanSpeedPercent;
                    xSemaphoreGive(settingsMutex);
                }
                oled_show_fan_settings(fanMenuIdx, editSpeed);
                lastMenuIdx = fanMenuIdx;
                // No `continue` — fall through so the triggering event is
                // processed immediately (BUG-031; same fix as incubation screen).
            }

            // Navigation mode
            if (fanEditState == F_NAV) {
                if (evt == UI_EVT_UP)   fanMenuIdx = (fanMenuIdx - 1 + 2) % 2;
                else if (evt == UI_EVT_DOWN) fanMenuIdx = (fanMenuIdx + 1) % 2;

                if (fanMenuIdx != lastMenuIdx) {
                    oled_show_fan_settings(fanMenuIdx, editSpeed);
                    lastMenuIdx = fanMenuIdx;
                }

                if (evt == UI_EVT_OK) {
                    if (fanMenuIdx == 0) {
                        fanEditState = F_EDIT_SPEED;
                    } else {
                        // Back
                        saveSettings();
                        uiState = UI_EGG_INCUBATOR_MENU; lastMenuIdx = -1;
                        oled_show_egg_incubator_menu(eggMenuIdx, eggMenuTop);
                        lastMenuIdx = eggMenuIdx;
                        continue;
                    }
                    oled_show_fan_settings(fanMenuIdx, editSpeed);
                }
            }

            // Edit speed mode
            else if (fanEditState == F_EDIT_SPEED) {
                if (evt == UI_EVT_UP) {
                    editSpeed = min((int)editSpeed + 5, 100);
                } else if (evt == UI_EVT_DOWN) {
                    editSpeed = max((int)editSpeed - 5, 0);
                } else if (evt == UI_EVT_OK) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        gSettings.fanSpeedPercent = editSpeed;
                        xSemaphoreGive(settingsMutex);
                    }
                    saveSettings();
                    fanEditState = F_NAV;
                }
                oled_show_fan_settings(fanMenuIdx, editSpeed);
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // EGG INCUBATOR MENU (10 items, 4 visible, scrollable)
        // 0-Control Mode, 1-Set Temp, 2-Set Hum, 3-Hysteresis, 4-Egg Type,
        // 5-Inc Day, 6-Turner, 7-Fan, 8-Pump Duration, 9-Back
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_EGG_INCUBATOR_MENU) {
            const int EGG_COUNT   = 10;
            const int EGG_VISIBLE = 4;
            if (evt == UI_EVT_UP) {
                eggMenuIdx = (eggMenuIdx - 1 + EGG_COUNT) % EGG_COUNT;
            } else if (evt == UI_EVT_DOWN) {
                eggMenuIdx = (eggMenuIdx + 1) % EGG_COUNT;
            }
            // Adjust window to keep selection visible (handles wrap-around too)
            if (eggMenuIdx < eggMenuTop)
                eggMenuTop = eggMenuIdx;
            else if (eggMenuIdx >= eggMenuTop + EGG_VISIBLE)
                eggMenuTop = eggMenuIdx - EGG_VISIBLE + 1;
            if (eggMenuTop < 0) eggMenuTop = 0;
            if (eggMenuTop > EGG_COUNT - EGG_VISIBLE) eggMenuTop = EGG_COUNT - EGG_VISIBLE;

            if (eggMenuIdx != lastMenuIdx) {
                oled_show_egg_incubator_menu(eggMenuIdx, eggMenuTop);
                lastMenuIdx = eggMenuIdx;
            }

            if (evt == UI_EVT_OK) {
                float curTemp = 0.0f, curHum = 0.0f;
                if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    curTemp = gSensorData.temp_ds18b20;
                    curHum  = gSensorData.humidity_dht;
                    xSemaphoreGive(sensorMutex);
                }
                switch (eggMenuIdx) {
                    case 0:  // Control Mode (Auto/Manual)
                        profileMenuParent = UI_EGG_INCUBATOR_MENU;
                        uiState = UI_MODE_MENU;
                        modeMenuIdx = (ctrlMode == MODE_AUTO) ? 0 : 1;
                        lastMenuIdx = -1;
                        oled_show_mode_menu(modeMenuIdx);
                        lastMenuIdx = modeMenuIdx;
                        break;
                    case 1:  // Set Temperature
                        profileMenuParent = UI_EGG_INCUBATOR_MENU;
                        uiState = UI_ENV_TEMPERATURE;
                        lastTempUiRefreshMs = 0;
                        oled_show_temperature(curTemp, tempSP);
                        break;
                    case 2:  // Set Humidity
                        profileMenuParent = UI_EGG_INCUBATOR_MENU;
                        uiState = UI_ENV_HUMIDITY;
                        oled_show_humidity(curHum, humSP);
                        break;
                    case 3:  // Hysteresis
                        profileMenuParent = UI_EGG_INCUBATOR_MENU;
                        uiState = UI_ENV_HYSTERESIS_MENU;
                        hysteresisMenuIdx = 0; lastMenuIdx = -1;
                        oled_show_hysteresis_menu(hysteresisMenuIdx, tempHyst, humHyst);
                        lastMenuIdx = hysteresisMenuIdx;
                        break;
                    case 4:  // Egg Type
                        uiState = UI_ENV_EGG_TYPE;
                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                            eggTypeIdx = (int)gSettings.eggType;
                            xSemaphoreGive(settingsMutex);
                        }
                        oled_show_egg_type(eggTypeIdx);
                        break;
                    case 5:  // Incubation Day
                        uiState = UI_ENV_INCUBATION_DAY;
                        // Draw right away — entering without drawing made the
                        // next press run the screen init instead of acting,
                        // so it took a double-press to enter (BUG-038).
                        enterIncubationScreen();
                        break;
                    case 6:  // Turner
                        uiState = UI_ENV_TURNER;
                        turnerMenuIdx = 0; lastMenuIdx = -1;
                        {
                            uint16_t intv = DEFAULT_TURNER_INTERVAL_MIN, dur = DEFAULT_TURNER_DURATION_SEC;
                            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                                intv = gSettings.turnerIntervalMin;
                                dur  = gSettings.turnerDurationSec;
                                xSemaphoreGive(settingsMutex);
                            }
                            oled_show_turner_settings(turnerMenuIdx, intv, dur, 0, false);
                        }
                        break;
                    case 7:  // Fan
                        uiState = UI_ENV_FAN;
                        fanMenuIdx = 0; lastMenuIdx = -1;
                        {
                            uint8_t speed = DEFAULT_FAN_SPEED_PERCENT;
                            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                                speed = (uint8_t)gSettings.fanSpeedPercent;
                                xSemaphoreGive(settingsMutex);
                            }
                            oled_show_fan_settings(fanMenuIdx, speed);
                        }
                        break;
                    case 8:  // Pump Duration
                        uiState = UI_PUMP_SETTINGS;
                        pumpMenuIdx = 0; pumpEditing = false;
                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                            editPumpDur = (uint16_t)gSettings.pumpDurationSec;
                            xSemaphoreGive(settingsMutex);
                        }
                        lastMenuIdx = -1;
                        oled_show_pump_settings(pumpMenuIdx, editPumpDur, pumpEditing);
                        lastMenuIdx = pumpMenuIdx;
                        break;
                    case 9:  // Back
                        uiState = UI_MAIN_MENU; lastMenuIdx = -1;
                        oled_show_menu(mainMenuIdx);
                        lastMenuIdx = mainMenuIdx;
                        break;
                    default: break;
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // CLIMATE CHAMBER MENU (6 items)
        // 0-Control Mode, 1-Set Temp, 2-Set Hum, 3-Hysteresis, 4-Climate Mode, 5-Back
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_CLIMATE_CHAMBER_MENU) {
            const int CLIM_COUNT   = 6;
            const int CLIM_VISIBLE = 4;
            if      (evt == UI_EVT_UP)   climMenuIdx = (climMenuIdx - 1 + CLIM_COUNT) % CLIM_COUNT;
            else if (evt == UI_EVT_DOWN) climMenuIdx = (climMenuIdx + 1) % CLIM_COUNT;
            if (climMenuIdx < climMenuTop)
                climMenuTop = climMenuIdx;
            else if (climMenuIdx >= climMenuTop + CLIM_VISIBLE)
                climMenuTop = climMenuIdx - CLIM_VISIBLE + 1;
            if (climMenuTop < 0) climMenuTop = 0;
            if (climMenuTop > CLIM_COUNT - CLIM_VISIBLE) climMenuTop = CLIM_COUNT - CLIM_VISIBLE;

            if (climMenuIdx != lastMenuIdx) {
                oled_show_climate_chamber_menu(climMenuIdx, climMenuTop);
                lastMenuIdx = climMenuIdx;
            }

            if (evt == UI_EVT_OK) {
                float curTemp = 0.0f, curHum = 0.0f;
                if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    curTemp = gSensorData.temp_ds18b20;
                    curHum  = gSensorData.humidity_dht;
                    xSemaphoreGive(sensorMutex);
                }
                switch (climMenuIdx) {
                    case 0:  // Control Mode
                        profileMenuParent = UI_CLIMATE_CHAMBER_MENU;
                        uiState = UI_MODE_MENU;
                        modeMenuIdx = (ctrlMode == MODE_AUTO) ? 0 : 1;
                        lastMenuIdx = -1;
                        oled_show_mode_menu(modeMenuIdx);
                        lastMenuIdx = modeMenuIdx;
                        break;
                    case 1:  // Set Temperature
                        profileMenuParent = UI_CLIMATE_CHAMBER_MENU;
                        uiState = UI_ENV_TEMPERATURE;
                        lastTempUiRefreshMs = 0;
                        oled_show_temperature(curTemp, tempSP);
                        break;
                    case 2:  // Set Humidity
                        profileMenuParent = UI_CLIMATE_CHAMBER_MENU;
                        uiState = UI_ENV_HUMIDITY;
                        oled_show_humidity(curHum, humSP);
                        break;
                    case 3:  // Hysteresis
                        profileMenuParent = UI_CLIMATE_CHAMBER_MENU;
                        uiState = UI_ENV_HYSTERESIS_MENU;
                        hysteresisMenuIdx = 0; lastMenuIdx = -1;
                        oled_show_hysteresis_menu(hysteresisMenuIdx, tempHyst, humHyst);
                        lastMenuIdx = hysteresisMenuIdx;
                        break;
                    case 4:  // Climate Mode
                        uiState = UI_CLIMATE_MODE_MENU;
                        climateModeIdx = 0; lastMenuIdx = -1;
                        oled_show_climate_mode_menu(climateModeIdx, climMode);
                        lastMenuIdx = climateModeIdx;
                        break;
                    case 5:  // Back
                        uiState = UI_MAIN_MENU; lastMenuIdx = -1;
                        oled_show_menu(mainMenuIdx);
                        lastMenuIdx = mainMenuIdx;
                        break;
                    default: break;
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // SYSTEM MENU (6 items)
        // 0-Switch Profile, 1-WiFi, 2-Time&Date, 3-Device Info, 4-Factory Reset, 5-Back
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_SYSTEM_MENU) {
            const int SYS_COUNT   = 6;
            const int SYS_VISIBLE = 4;
            if      (evt == UI_EVT_UP)   sysMenuIdx = (sysMenuIdx - 1 + SYS_COUNT) % SYS_COUNT;
            else if (evt == UI_EVT_DOWN) sysMenuIdx = (sysMenuIdx + 1) % SYS_COUNT;
            if (sysMenuIdx < sysMenuTop)
                sysMenuTop = sysMenuIdx;
            else if (sysMenuIdx >= sysMenuTop + SYS_VISIBLE)
                sysMenuTop = sysMenuIdx - SYS_VISIBLE + 1;
            if (sysMenuTop < 0) sysMenuTop = 0;
            if (sysMenuTop > SYS_COUNT - SYS_VISIBLE) sysMenuTop = SYS_COUNT - SYS_VISIBLE;

            if (sysMenuIdx != lastMenuIdx) {
                oled_show_system_menu(sysMenuIdx, sysMenuTop);
                lastMenuIdx = sysMenuIdx;
            }

            if (evt == UI_EVT_OK) {
                switch (sysMenuIdx) {
                    case 0:  // Switch Profile
                        uiState = UI_CONTROLLER_MODE_MENU;
                        controllerModeIdx = 0; lastMenuIdx = -1;
                        oled_show_controller_mode(controllerModeIdx, profile);
                        lastMenuIdx = controllerModeIdx;
                        break;
                    case 1:  // WiFi
                        uiState = UI_WIFI_MENU;
                        wifiMenuIdx = 0; lastMenuIdx = -1;
                        oled_show_wifi_menu(wifiMenuIdx, WiFi.status() == WL_CONNECTED);
                        lastMenuIdx = wifiMenuIdx;
                        break;
                    case 2:  // Time & Date
                        timeDateMenuIdx = 0;
                        uiState = UI_TIME_DATE_MENU;
                        lastMenuIdx = -1;
                        oled_show_time_date_menu(timeDateMenuIdx);
                        lastMenuIdx = timeDateMenuIdx;
                        break;
                    case 3: {  // Device Info
                        uiState = UI_DEVICE_INFO;
                        String ip = WiFi.localIP().toString();
                        uint32_t up = millis() / 1000;
                        oled_show_device_info(DEVICE_ID, FW_VERSION, ip.c_str(),
                            profile == PROFILE_EGG_INCUBATOR ? "Incubator" : "Climate", up);
                        break;
                    }
                    case 4:  // Factory Reset
                        uiState = UI_FACTORY_RESET_CONFIRM;
                        factoryResetIdx = 0;  // default to No (BUG-034)
                        oled_show_factory_reset_confirm(factoryResetIdx);
                        break;
                    case 5:  // Back
                        uiState = UI_MAIN_MENU; lastMenuIdx = -1;
                        oled_show_menu(mainMenuIdx);
                        lastMenuIdx = mainMenuIdx;
                        break;
                    default: break;
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // PUMP SETTINGS (0=Duration, 1=Back)
        // Two-layer: navigation mode + edit mode (±5 s steps, 5-120 s bounds)
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_PUMP_SETTINGS) {
            if (!pumpEditing) {
                if      (evt == UI_EVT_UP)   pumpMenuIdx = (pumpMenuIdx - 1 + 2) % 2;
                else if (evt == UI_EVT_DOWN) pumpMenuIdx = (pumpMenuIdx + 1) % 2;

                if (pumpMenuIdx != lastMenuIdx) {
                    oled_show_pump_settings(pumpMenuIdx, editPumpDur, false);
                    lastMenuIdx = pumpMenuIdx;
                }

                if (evt == UI_EVT_OK) {
                    if (pumpMenuIdx == 0) {
                        pumpEditing = true;
                        oled_show_pump_settings(pumpMenuIdx, editPumpDur, true);
                    } else {
                        // Back — restore scroll position to Pump Duration row
                        uiState = UI_EGG_INCUBATOR_MENU;
                        eggMenuIdx = 8; eggMenuTop = 6;
                        lastMenuIdx = -1;
                        oled_show_egg_incubator_menu(eggMenuIdx, eggMenuTop);
                        lastMenuIdx = eggMenuIdx;
                    }
                }
            } else {
                // Editing duration
                if (evt == UI_EVT_UP) {
                    editPumpDur = (uint16_t)min((int)editPumpDur + 5, 120);
                } else if (evt == UI_EVT_DOWN) {
                    editPumpDur = (uint16_t)max((int)editPumpDur - 5, 5);
                } else if (evt == UI_EVT_OK) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        gSettings.pumpDurationSec = editPumpDur;
                        xSemaphoreGive(settingsMutex);
                    }
                    {
                        Preferences prefs;
                        prefs.begin("incubator", false);
                        prefs.putUInt("pumpDur", editPumpDur);
                        prefs.end();
                    }
                    pumpEditing = false;
                }
                oled_show_pump_settings(pumpMenuIdx, editPumpDur, pumpEditing);
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // CLIMATE MODE MENU
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_CLIMATE_MODE_MENU) {
            if      (evt == UI_EVT_UP)   climateModeIdx = (climateModeIdx - 1 + 4) % 4;
            else if (evt == UI_EVT_DOWN) climateModeIdx = (climateModeIdx + 1) % 4;

            if (climateModeIdx != lastMenuIdx) {
                oled_show_climate_mode_menu(climateModeIdx, climMode);
                lastMenuIdx = climateModeIdx;
            }

            if (evt == UI_EVT_OK) {
                if (climateModeIdx == 0) {
                    // Load edit buffers only — mode and hours are committed
                    // together on the editor's final OK (BUG-035).
                    uiState = UI_CLIMATE_SCHEDULE;
                    climateSchedIdx = 0;
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        editSchedStart = gSettings.schedStartHour;
                        editSchedEnd   = gSettings.schedEndHour;
                        xSemaphoreGive(settingsMutex);
                    }
                    oled_show_climate_schedule(climateSchedIdx, editSchedStart, editSchedEnd);
                } else if (climateModeIdx == 1) {
                    // Same edit-buffer pattern (BUG-035).
                    uiState = UI_CLIMATE_CYCLIC;
                    climateCyclicIdx = 0;
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        editHeatPeriod = gSettings.heatPeriodMin;
                        editCoolPeriod = gSettings.coolPeriodMin;
                        xSemaphoreGive(settingsMutex);
                    }
                    oled_show_climate_cyclic(climateCyclicIdx, editHeatPeriod, editCoolPeriod);
                } else if (climateModeIdx == 2) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.climateMode      = CLIMATE_RAMP;
                        gSettings.rampStepIdx      = 0;
                        gSettings.rampStepStartEpoch = 0;
                        xSemaphoreGive(settingsMutex);
                    }
                    uiState = UI_CLIMATE_RAMP;
                    climateRampIdx = 0;
                    RampStep_t rs[CLIMATE_MAX_RAMP_STEPS];
                    uint8_t sc = 0, asi = 0;
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        sc  = gSettings.rampCount;
                        asi = gSettings.rampStepIdx;
                        memcpy(rs, gSettings.rampSteps, sizeof(rs));
                        xSemaphoreGive(settingsMutex);
                    }
                    oled_show_climate_ramp(climateRampIdx, sc, rs, asi);
                } else {
                    uiState = UI_CLIMATE_CHAMBER_MENU; lastMenuIdx = -1;
                    oled_show_climate_chamber_menu(climMenuIdx, climMenuTop);
                    lastMenuIdx = climMenuIdx;
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // CLIMATE FIXED SCHEDULE EDIT (0=start, 1=end, 2=back)
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_CLIMATE_SCHEDULE) {
            // BUG-035: UP/DOWN edit local buffers; gSettings (incl. the mode
            // switch itself) is written only on the final OK.
            if (climateSchedIdx == 0) {
                if      (evt == UI_EVT_UP)   editSchedStart = (editSchedStart + 1) % 24;
                else if (evt == UI_EVT_DOWN) editSchedStart = (editSchedStart + 23) % 24;
            } else if (climateSchedIdx == 1) {
                if      (evt == UI_EVT_UP)   editSchedEnd = (editSchedEnd + 1) % 24;
                else if (evt == UI_EVT_DOWN) editSchedEnd = (editSchedEnd + 23) % 24;
            }

            if (evt == UI_EVT_OK) {
                if (climateSchedIdx < 2) {
                    climateSchedIdx++;
                } else {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        gSettings.schedStartHour = editSchedStart;
                        gSettings.schedEndHour   = editSchedEnd;
                        gSettings.climateMode    = CLIMATE_FIXED_SCHEDULE;
                        xSemaphoreGive(settingsMutex);
                    }
                    saveSettings();
                    uiState = UI_CLIMATE_MODE_MENU; lastMenuIdx = -1;
                    oled_show_climate_mode_menu(climateModeIdx, CLIMATE_FIXED_SCHEDULE);
                    lastMenuIdx = climateModeIdx;
                    continue;
                }
            }
            oled_show_climate_schedule(climateSchedIdx, editSchedStart, editSchedEnd);
        }

        // ═════════════════════════════════════════════════════════════════════
        // CLIMATE CYCLIC EDIT (0=heat period, 1=cool period, 2=back)
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_CLIMATE_CYCLIC) {
            // BUG-035: UP/DOWN edit local buffers; gSettings (incl. the mode
            // switch and cycle restart) is written only on the final OK.
            if (climateCyclicIdx == 0) {
                if      (evt == UI_EVT_UP)   editHeatPeriod = min(editHeatPeriod + 30UL, 1440UL);
                else if (evt == UI_EVT_DOWN) { if (editHeatPeriod > 30) editHeatPeriod -= 30; }
            } else if (climateCyclicIdx == 1) {
                if      (evt == UI_EVT_UP)   editCoolPeriod = min(editCoolPeriod + 30UL, 1440UL);
                else if (evt == UI_EVT_DOWN) { if (editCoolPeriod > 30) editCoolPeriod -= 30; }
            }

            if (evt == UI_EVT_OK) {
                if (climateCyclicIdx < 2) {
                    climateCyclicIdx++;
                } else {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        gSettings.heatPeriodMin   = editHeatPeriod;
                        gSettings.coolPeriodMin   = editCoolPeriod;
                        gSettings.climateMode     = CLIMATE_CYCLIC;
                        gSettings.cycleStartEpoch = 0;  // restart cycle on commit
                        xSemaphoreGive(settingsMutex);
                    }
                    saveSettings();
                    uiState = UI_CLIMATE_MODE_MENU; lastMenuIdx = -1;
                    oled_show_climate_mode_menu(climateModeIdx, CLIMATE_CYCLIC);
                    lastMenuIdx = climateModeIdx;
                    continue;
                }
            }
            oled_show_climate_cyclic(climateCyclicIdx, editHeatPeriod, editCoolPeriod);
        }

        // ═════════════════════════════════════════════════════════════════════
        // CLIMATE RAMP — view steps, back only (ramp editing is advanced,
        // handled via pre-configured steps in NVS or future extension)
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_CLIMATE_RAMP) {
            RampStep_t rs[CLIMATE_MAX_RAMP_STEPS];
            uint8_t sc = 0, asi = 0;
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                sc  = gSettings.rampCount;
                asi = gSettings.rampStepIdx;
                memcpy(rs, gSettings.rampSteps, sizeof(rs));
                xSemaphoreGive(settingsMutex);
            }
            int maxIdx = (int)sc; // last item = Back
            if (evt == UI_EVT_UP)   climateRampIdx = max(0, climateRampIdx - 1);
            if (evt == UI_EVT_DOWN) climateRampIdx = min(maxIdx, climateRampIdx + 1);
            if (evt == UI_EVT_OK && climateRampIdx == maxIdx) {
                uiState = UI_CLIMATE_MODE_MENU; lastMenuIdx = -1;
                oled_show_climate_mode_menu(climateModeIdx, climMode);
                lastMenuIdx = climateModeIdx;
                continue;
            }
            oled_show_climate_ramp(climateRampIdx, sc, rs, asi);
        }

        // (BUG-025: dead UI_SETTINGS_MENU handler removed — no state ever entered it)

        // ═════════════════════════════════════════════════════════════════════
        // WiFi MENU  (Settings → WiFi)
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_WIFI_MENU) {
            bool connected = (WiFi.status() == WL_CONNECTED);
            if      (evt == UI_EVT_UP)   wifiMenuIdx = (wifiMenuIdx - 1 + 2) % 2;
            else if (evt == UI_EVT_DOWN) wifiMenuIdx = (wifiMenuIdx + 1) % 2;

            if (wifiMenuIdx != lastMenuIdx) {
                oled_show_wifi_menu(wifiMenuIdx, connected);
                lastMenuIdx = wifiMenuIdx;
            }

            if (evt == UI_EVT_OK) {
                if (wifiMenuIdx == 0) {
                    // Toggle connect / disconnect
                    if (connected) {
                        wifi_request_disconnect();
                    } else {
                        wifi_request_connect();
                    }
                    // Refresh display immediately to reflect the new (requested) state
                    bool nowConnected = (WiFi.status() == WL_CONNECTED);
                    oled_show_wifi_menu(wifiMenuIdx, nowConnected);
                    lastMenuIdx = wifiMenuIdx;
                } else {
                    // Back
                    uiState = UI_SYSTEM_MENU;
                    lastMenuIdx = -1;
                    oled_show_system_menu(sysMenuIdx, sysMenuTop);
                    lastMenuIdx = sysMenuIdx;
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // MODE MENU (Auto / Manual / Back)
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_MODE_MENU) {
            if      (evt == UI_EVT_UP)   modeMenuIdx = (modeMenuIdx - 1 + 3) % 3;
            else if (evt == UI_EVT_DOWN) modeMenuIdx = (modeMenuIdx + 1) % 3;

            if (modeMenuIdx != lastMenuIdx) {
                oled_show_mode_menu(modeMenuIdx);
                lastMenuIdx = modeMenuIdx;
            }

            if (evt == UI_EVT_OK) {
                if (modeMenuIdx == 0) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.controlMode = MODE_AUTO;
                        xSemaphoreGive(settingsMutex);
                    }
                    saveSettings();
                    if (profileMenuParent == UI_CLIMATE_CHAMBER_MENU) {
                        uiState = UI_CLIMATE_CHAMBER_MENU; lastMenuIdx = -1;
                        oled_show_climate_chamber_menu(climMenuIdx, climMenuTop);
                        lastMenuIdx = climMenuIdx;
                    } else {
                        uiState = UI_EGG_INCUBATOR_MENU; lastMenuIdx = -1;
                        oled_show_egg_incubator_menu(eggMenuIdx, eggMenuTop);
                        lastMenuIdx = eggMenuIdx;
                    }
                } else if (modeMenuIdx == 1) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.controlMode = MODE_MANUAL;
                        xSemaphoreGive(settingsMutex);
                    }
                    saveSettings();
                    uiState = UI_MANUAL_CONTROL_MENU;
                    manualCtrlIdx = 0; lastMenuIdx = -1;
                    bool hm = false, cm = false;
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        hm = gSettings.heaterManualOn;
                        cm = gSettings.coolerManualOn;
                        xSemaphoreGive(settingsMutex);
                    }
                    oled_show_manual_control(manualCtrlIdx, hm, cm, profile);
                    lastMenuIdx = manualCtrlIdx;
                } else {
                    if (profileMenuParent == UI_CLIMATE_CHAMBER_MENU) {
                        uiState = UI_CLIMATE_CHAMBER_MENU; lastMenuIdx = -1;
                        oled_show_climate_chamber_menu(climMenuIdx, climMenuTop);
                        lastMenuIdx = climMenuIdx;
                    } else {
                        uiState = UI_EGG_INCUBATOR_MENU; lastMenuIdx = -1;
                        oled_show_egg_incubator_menu(eggMenuIdx, eggMenuTop);
                        lastMenuIdx = eggMenuIdx;
                    }
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // MANUAL CONTROL MENU
        // incubator: 0=heater, 1=back
        // climate:   0=heater, 1=cooler, 2=back
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_MANUAL_CONTROL_MENU) {
            int items = (profile == PROFILE_CLIMATE_CHAMBER) ? 3 : 2;
            if      (evt == UI_EVT_UP)   manualCtrlIdx = (manualCtrlIdx - 1 + items) % items;
            else if (evt == UI_EVT_DOWN) manualCtrlIdx = (manualCtrlIdx + 1) % items;

            bool hm = false, cm = false;
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                hm = gSettings.heaterManualOn;
                cm = gSettings.coolerManualOn;
                xSemaphoreGive(settingsMutex);
            }

            if (manualCtrlIdx != lastMenuIdx) {
                oled_show_manual_control(manualCtrlIdx, hm, cm, profile);
                lastMenuIdx = manualCtrlIdx;
            }

            if (evt == UI_EVT_OK) {
                int backIdx = (profile == PROFILE_CLIMATE_CHAMBER) ? 2 : 1;

                if (manualCtrlIdx == backIdx) {
                    // Save only on exit (not on every toggle)
                    saveSettings();
                    uiState = UI_MODE_MENU; lastMenuIdx = -1;
                    oled_show_mode_menu(modeMenuIdx);
                    lastMenuIdx = modeMenuIdx;
                } else if (manualCtrlIdx == 0) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.heaterManualOn = !gSettings.heaterManualOn;
                        hm = gSettings.heaterManualOn;
                        xSemaphoreGive(settingsMutex);
                    }
                    oled_show_manual_control(manualCtrlIdx, hm, cm, profile);
                } else if (manualCtrlIdx == 1 && profile == PROFILE_CLIMATE_CHAMBER) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.coolerManualOn = !gSettings.coolerManualOn;
                        cm = gSettings.coolerManualOn;
                        xSemaphoreGive(settingsMutex);
                    }
                    oled_show_manual_control(manualCtrlIdx, hm, cm, profile);
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // DEVICE INFO (any button exits)
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_DEVICE_INFO) {
            if (evt == UI_EVT_OK || evt == UI_EVT_DOWN) {
                uiState = UI_SYSTEM_MENU; lastMenuIdx = -1;
                oled_show_system_menu(sysMenuIdx, sysMenuTop);
                lastMenuIdx = sysMenuIdx;
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // FACTORY RESET CONFIRM
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_FACTORY_RESET_CONFIRM) {
            // BUG-034: explicit No/Yes selection, defaulting to No, so a
            // single (or replayed) OK press cannot wipe the device.
            if (evt == UI_EVT_UP || evt == UI_EVT_DOWN) {
                factoryResetIdx = 1 - factoryResetIdx;
                oled_show_factory_reset_confirm(factoryResetIdx);
            } else if (evt == UI_EVT_OK) {
                if (factoryResetIdx == 1) {
                    // Wipe NVS and restart
                    extern void factoryReset(void);
                    factoryReset();
                } else {
                    uiState = UI_SYSTEM_MENU; lastMenuIdx = -1;
                    oled_show_system_menu(sysMenuIdx, sysMenuTop);
                    lastMenuIdx = sysMenuIdx;
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════
        // TIME & DATE MENU  (Settings → Time & Date)
        // Items: 0=Manual Set  1=WiFi Sync  2=Back
        // ═════════════════════════════════════════════════════════════════
        else if (uiState == UI_TIME_DATE_MENU) {
            if      (evt == UI_EVT_UP)   timeDateMenuIdx = (timeDateMenuIdx - 1 + 3) % 3;
            else if (evt == UI_EVT_DOWN) timeDateMenuIdx = (timeDateMenuIdx + 1) % 3;

            if (timeDateMenuIdx != lastMenuIdx) {
                oled_show_time_date_menu(timeDateMenuIdx);
                lastMenuIdx = timeDateMenuIdx;
            }

            if (evt == UI_EVT_OK) {
                if (timeDateMenuIdx == 0) {
                    // MANUAL SET: seed edit buffer from current RTC time
                    DateTime now;
                    if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        now = gRtcTime.now;
                        xSemaphoreGive(rtcMutex);
                    }
                    tH  = now.hour();   tM  = now.minute(); tS = now.second();
                    tD  = now.day();    tMo = now.month();  tY = now.year();
                    tEditField = 0;
                    uiState = UI_TIME_MANUAL_EDIT;
                    lastMenuIdx = -1;
                    oled_show_time_edit(tEditField, tH, tM, tS, tD, tMo, tY);

                } else if (timeDateMenuIdx == 1) {
                    // WIFI SYNC: post request to wifi task, then hand off to the
                    // non-blocking UI_TIME_WIFI_SYNC state — buttons stay live and
                    // no presses are queued up for replay (BUG-033).
                    wifi_request_ntp_sync();
                    ntpSyncStartMs = millis();
                    ntpSyncResult  = 0;
                    uiState = UI_TIME_WIFI_SYNC;
                    oled_show_time_wifi_sync(0);  // "Syncing Time..."

                } else {
                    // BACK
                    uiState = UI_SYSTEM_MENU;
                    lastMenuIdx = -1;
                    oled_show_system_menu(sysMenuIdx, sysMenuTop);
                    lastMenuIdx = sysMenuIdx;
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════
        // TIME MANUAL EDIT
        // field 0-5: H/M/S/D/Mo/Y  —  field 6: SAVE ? prompt
        // UP/DOWN change value, OK advances field, OK on field-6 saves to RTC
        // DOWN on field-6 cancels without saving
        // ═════════════════════════════════════════════════════════════════
        else if (uiState == UI_TIME_MANUAL_EDIT) {
            if (tEditField < 6) {
                // Editing a field value
                if (evt == UI_EVT_UP) {
                    switch (tEditField) {
                        case 0: tH  = (tH  + 1) % 24;                        break;
                        case 1: tM  = (tM  + 1) % 60;                        break;
                        case 2: tS  = (tS  + 1) % 60;                        break;
                        case 3: tD  = (tD % daysInMonth(tMo, tY)) + 1;       break;
                        case 4: tMo = (tMo % 12) + 1; if (tD > daysInMonth(tMo, tY)) tD = daysInMonth(tMo, tY); break;
                        case 5: tY  = min(2099, tY + 1); if (tD > daysInMonth(tMo, tY)) tD = daysInMonth(tMo, tY); break;
                    }
                } else if (evt == UI_EVT_DOWN) {
                    switch (tEditField) {
                        case 0: tH  = (tH  - 1 + 24) % 24;                   break;
                        case 1: tM  = (tM  - 1 + 60) % 60;                   break;
                        case 2: tS  = (tS  - 1 + 60) % 60;                   break;
                        case 3: tD  = (tD <= 1) ? daysInMonth(tMo, tY) : tD - 1; break;
                        case 4: tMo = (tMo <= 1) ? 12 : tMo - 1; if (tD > daysInMonth(tMo, tY)) tD = daysInMonth(tMo, tY); break;
                        case 5: tY  = max(2000, tY - 1); if (tD > daysInMonth(tMo, tY)) tD = daysInMonth(tMo, tY); break;
                    }
                } else if (evt == UI_EVT_OK) {
                    // advance to next field
                    tEditField++;
                }
                oled_show_time_edit(tEditField, tH, tM, tS, tD, tMo, tY);

            } else {
                // field == 6: SAVE? prompt
                if (evt == UI_EVT_OK) {
                    // Write to DS1307 RTC (clamp day defensively before adjust)
                    if (tD > daysInMonth(tMo, tY)) tD = daysInMonth(tMo, tY);
                    DateTime newDt(tY, tMo, tD, tH, tM, tS);
                    rtc.adjust(newDt);
                    clampEpochsToNow(newDt.unixtime());
                    Serial.printf("[RTC] Manually set to %04d-%02d-%02d %02d:%02d:%02d\n",
                                  tY, tMo, tD, tH, tM, tS);
                    uiState = UI_SYSTEM_MENU;
                    lastMenuIdx = -1;
                    oled_show_system_menu(sysMenuIdx, sysMenuTop);
                    lastMenuIdx = sysMenuIdx;
                } else if (evt == UI_EVT_DOWN) {
                    // Cancel — discard edits
                    uiState = UI_SYSTEM_MENU;
                    lastMenuIdx = -1;
                    oled_show_system_menu(sysMenuIdx, sysMenuTop);
                    lastMenuIdx = sysMenuIdx;
                }
                // UP has no effect on save prompt
            }
        }

        // ═════════════════════════════════════════════════════════════════
        // TIME WIFI SYNC — non-blocking (BUG-033)
        // Progress is driven by ntpSyncPoll() on idle ticks; while the sync is
        // pending, button presses are consumed and ignored. Once the result is
        // shown, any button dismisses it early (auto-dismiss after 1.5 s).
        // ═════════════════════════════════════════════════════════════════
        else if (uiState == UI_TIME_WIFI_SYNC) {
            ntpSyncPoll();
            if (uiState == UI_TIME_WIFI_SYNC && ntpSyncResult != 0 &&
                (evt == UI_EVT_OK || evt == UI_EVT_DOWN || evt == UI_EVT_UP)) {
                drainUiQueue();
                uiState = UI_SYSTEM_MENU;
                lastMenuIdx = -1;
                oled_show_system_menu(sysMenuIdx, sysMenuTop);
                lastMenuIdx = sysMenuIdx;
            }
        }

    } // end for(;;)
}
