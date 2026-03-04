#include "task_ui.h"
#include "globals.h"
#include "config.h"
#include "oled_ui.h"
#include "incubator_logic.h"
#include "task_incubator.h"
#include "task_wifi_manager.h"
#include <WiFi.h>
#include <Arduino.h>
#include <time.h>

extern RTC_DS1307 rtc;  // defined in egg_incubator_v2.ino
 
// Helper: stringify UI events/states for logging
static const char* uiEventName(UiEvent e) {
    switch (e) {
        case UI_EVT_NONE: return "NONE";
        case UI_EVT_UP:   return "UP";
        case UI_EVT_DOWN: return "DOWN";
        case UI_EVT_OK:   return "OK";
        case UI_EVT_LONGOK: return "LONGOK";
        default: return "?";
    }
}

static const char* uiStateName(UiState s) {
    switch (s) {
        case UI_HOME: return "HOME";
        case UI_MAIN_MENU: return "MAIN_MENU";
        case UI_CONTROLLER_MODE_MENU: return "CONTROLLER_MODE_MENU";
        case UI_SET_ENV_MENU: return "SET_ENV_MENU";
        case UI_SETTINGS_MENU: return "SETTINGS_MENU";
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
static int      envMenuIdx       = 0;
static int      settingsMenuIdx  = 0;
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

// ─────────────────────────────────────────────────────────────────────────────
// TASK: UI STATE MACHINE
// ─────────────────────────────────────────────────────────────────────────────
void task_ui(void* pvParameters) {
    oled_init();

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
                        snprintf(phase, sizeof(phase), rs.heaterOn ? "HEAT" : "COOL");
                    } else if (climMode == CLIMATE_CYCLIC) {
                        snprintf(phase, sizeof(phase), rs.heaterOn ? "HEAT" : "COOL");
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
                    case MENU_CONTROLLER_MODE:
                        uiState = UI_CONTROLLER_MODE_MENU;
                        controllerModeIdx = 0; lastMenuIdx = -1;
                        oled_show_controller_mode(controllerModeIdx, profile);
                        lastMenuIdx = controllerModeIdx;
                        break;
                    case MENU_SET_ENVIRONMENT:
                        uiState = UI_SET_ENV_MENU;
                        envMenuIdx = 0; lastMenuIdx = -1;
                        oled_show_set_environment(envMenuIdx, profile);
                        lastMenuIdx = envMenuIdx;
                        break;
                    case MENU_SETTINGS:
                        uiState = UI_SETTINGS_MENU;
                        settingsMenuIdx = 0; lastMenuIdx = -1;
                        oled_show_settings_menu(settingsMenuIdx);
                        lastMenuIdx = settingsMenuIdx;
                        break;
                    case MENU_EXIT:
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
                    uiState = UI_MAIN_MENU; lastMenuIdx = -1;
                    oled_show_menu(mainMenuIdx);
                    lastMenuIdx = mainMenuIdx;
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // SET ENVIRONMENT MENU
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_SET_ENV_MENU) {
            // Number of items depends on profile:
            // Incubator: Temperature, Hysteresis, Humidity, Inc.Start Day, Egg Type, Turner, Fan, Back = 8
            // Climate:   Temperature, Hysteresis, Humidity, Climate Profile, Back = 5
            int itemCount = (profile == PROFILE_EGG_INCUBATOR) ? 8 : 5;
            if      (evt == UI_EVT_UP)   envMenuIdx = (envMenuIdx - 1 + itemCount) % itemCount;
            else if (evt == UI_EVT_DOWN) envMenuIdx = (envMenuIdx + 1) % itemCount;

            if (envMenuIdx != lastMenuIdx) {
                oled_show_set_environment(envMenuIdx, profile);
                lastMenuIdx = envMenuIdx;
            }

            if (evt == UI_EVT_OK) {
                float curTemp = 0.0f;
                float curHum  = 0.0f;
                if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    curTemp = gSensorData.temp_ds18b20;
                    curHum  = gSensorData.humidity_dht;
                    xSemaphoreGive(sensorMutex);
                }

                // Common items 0-2
                if (envMenuIdx == 0) {
                    uiState = UI_ENV_TEMPERATURE;
                    lastTempUiRefreshMs = 0;
                    oled_show_temperature(curTemp, tempSP);
                } else if (envMenuIdx == 1) {
                    uiState = UI_ENV_HYSTERESIS_MENU;
                    hysteresisMenuIdx = 0; lastMenuIdx = -1;
                    oled_show_hysteresis_menu(hysteresisMenuIdx, tempHyst, humHyst);
                    lastMenuIdx = hysteresisMenuIdx;
                } else if (envMenuIdx == 2) {
                    uiState = UI_ENV_HUMIDITY;
                    oled_show_humidity(curHum, humSP);
                }

                // Egg incubator items 3-7
                else if (profile == PROFILE_EGG_INCUBATOR) {
                    if (envMenuIdx == 3) {
                    // Incubation start date
                        uiState = UI_ENV_INCUBATION_DAY;
                        // Force the UI state's own initialization path to run on the
                        // next loop iteration so static edit/navigation state is reset
                        // (prevents stale mode/editField from previous visits).
                        editField = 0;
                        lastMenuIdx = -1;
                        Serial.println("[UI] Request enter INCUBATION screen (will init next loop)");
                        continue;
                    } else if (envMenuIdx == 4) {
                        uiState = UI_ENV_EGG_TYPE;
                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                            eggTypeIdx = (int)gSettings.eggType;
                            xSemaphoreGive(settingsMutex);
                        }
                        oled_show_egg_type(eggTypeIdx);
                    } else if (envMenuIdx == 5) {
                        uiState = UI_ENV_TURNER;
                        turnerMenuIdx = 0;
                        uint16_t intv = DEFAULT_TURNER_INTERVAL_MIN, dur = DEFAULT_TURNER_DURATION_SEC;
                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                            intv = gSettings.turnerIntervalMin;
                            dur  = gSettings.turnerDurationSec;
                            xSemaphoreGive(settingsMutex);
                        }
                        oled_show_turner_settings(turnerMenuIdx, intv, dur, 0, false);
                    } else if (envMenuIdx == 6) {
                        uiState = UI_ENV_FAN;
                        fanMenuIdx = 0;
                        uint8_t speed = DEFAULT_FAN_SPEED_PERCENT;
                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                            speed = (uint8_t)gSettings.fanSpeedPercent;
                            xSemaphoreGive(settingsMutex);
                        }
                        oled_show_fan_settings(fanMenuIdx, speed);
                    } else if (envMenuIdx == 7) {
                        // Back
                        uiState = UI_MAIN_MENU; lastMenuIdx = -1;
                        oled_show_menu(mainMenuIdx);
                        lastMenuIdx = mainMenuIdx;
                    }
                }

                // Climate chamber items 3-4
                else {
                    if (envMenuIdx == 3) {
                        uiState = UI_CLIMATE_MODE_MENU;
                        climateModeIdx = 0; lastMenuIdx = -1;
                        oled_show_climate_mode_menu(climateModeIdx, climMode);
                        lastMenuIdx = climateModeIdx;
                    } else if (envMenuIdx == 4) {
                        uiState = UI_MAIN_MENU; lastMenuIdx = -1;
                        oled_show_menu(mainMenuIdx);
                        lastMenuIdx = mainMenuIdx;
                    }
                }
            }
        }

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
                }
                redraw = true;
            } else if (evt == UI_EVT_DOWN) {
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    gSettings.tempSetpoint = round1(gSettings.tempSetpoint - TEMP_SETPOINT_STEP);
                    if (gSettings.tempSetpoint < TEMP_SETPOINT_MIN) gSettings.tempSetpoint = TEMP_SETPOINT_MIN;
                    tempSP = gSettings.tempSetpoint;
                    xSemaphoreGive(settingsMutex);
                }
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
                uiState = UI_SET_ENV_MENU; lastMenuIdx = -1;
                oled_show_set_environment(envMenuIdx, profile);
                lastMenuIdx = envMenuIdx;
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
                }
                redraw = true;
            } else if (evt == UI_EVT_DOWN) {
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    gSettings.humSetpoint -= HUM_SETPOINT_STEP;
                    if (gSettings.humSetpoint < HUM_SETPOINT_MIN) gSettings.humSetpoint = HUM_SETPOINT_MIN;
                    humSP = gSettings.humSetpoint;
                    xSemaphoreGive(settingsMutex);
                }
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
                uiState = UI_SET_ENV_MENU; lastMenuIdx = -1;
                oled_show_set_environment(envMenuIdx, profile);
                lastMenuIdx = envMenuIdx;
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
                    uiState = UI_SET_ENV_MENU; lastMenuIdx = -1;
                    oled_show_set_environment(envMenuIdx, profile);
                    lastMenuIdx = envMenuIdx;
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
                }
            } else if (evt == UI_EVT_DOWN) {
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    gSettings.tempHysteresis = round1(gSettings.tempHysteresis - 0.1f);
                    if (gSettings.tempHysteresis < TEMP_HYST_MIN) gSettings.tempHysteresis = TEMP_HYST_MIN;
                    tempHyst = gSettings.tempHysteresis;
                    xSemaphoreGive(settingsMutex);
                }
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
                }
            } else if (evt == UI_EVT_DOWN) {
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                    gSettings.humHysteresis -= 1.0f;
                    if (gSettings.humHysteresis < HUM_HYST_MIN) gSettings.humHysteresis = HUM_HYST_MIN;
                    humHyst = gSettings.humHysteresis;
                    xSemaphoreGive(settingsMutex);
                }
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
                uiState = UI_SET_ENV_MENU; lastMenuIdx = -1;
                oled_show_set_environment(envMenuIdx, profile);
                lastMenuIdx = envMenuIdx;
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // INCUBATION START DATE (two-item menu: Start Date / Back)
        // Navigation mode vs Edit mode (separate)
        else if (uiState == UI_ENV_INCUBATION_DAY) {
            enum IncubMode { IM_NAV = 0, IM_EDIT };
            static IncubMode mode = IM_NAV;
            static int navIdx = 0; // 0=Start Date, 1=Back
            static int dispDay = 1, dispMonth = 1, dispYear = 2024;

            // Initialize screen on entry
            if (lastMenuIdx == -1) {
                Serial.println("[UI] INCUBATION screen init");
                lastOkUiMs = 0;  // clear rate-limiter on every fresh entry
                // load snapshot from settings or RTC
                uint32_t sEpoch = 0;
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    sEpoch = gSettings.startEpoch;
                    xSemaphoreGive(settingsMutex);
                }
                if (sEpoch != 0) {
                    DateTime sd((uint32_t)sEpoch);
                    dispDay = sd.day(); dispMonth = sd.month(); dispYear = sd.year();
                } else {
                    DateTime now(2024,1,1);
                    if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        now = gRtcTime.now;
                        xSemaphoreGive(rtcMutex);
                    }
                    dispDay = now.day(); dispMonth = now.month(); dispYear = now.year();
                }
                // Always start in navigation mode on entry; do not preload edit values
                mode = IM_NAV; navIdx = 0; editField = 0;
                lastMenuIdx = navIdx;
                oled_show_incubation_day_set(navIdx, dispDay, dispMonth, dispYear, false, 0);
                // Do NOT continue here — fall through so the triggering event
                // is processed immediately by the IM_NAV handler below.
                // (Previously this `continue` caused the first button press to
                // be silently discarded, requiring a double-press to enter.)
            }

            if (mode == IM_NAV) {
                // navigate between Start Date and Back
                if (evt == UI_EVT_UP)   navIdx = (navIdx - 1 + 2) % 2;
                else if (evt == UI_EVT_DOWN) navIdx = (navIdx + 1) % 2;

                // always redraw navigation row so a single UP/DOWN press updates immediately
                oled_show_incubation_day_set(navIdx, dispDay, dispMonth, dispYear, false, 0);
                lastMenuIdx = navIdx;

                if (evt == UI_EVT_OK) {
                    if (navIdx == 0) {
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
                        mode = IM_EDIT;
                        oled_show_incubation_day_set(0, editDay, editMonth, editYear, true, editField);
                    } else {
                        // Back to Set Environment
                        uiState = UI_SET_ENV_MENU; lastMenuIdx = -1;
                        oled_show_set_environment(envMenuIdx, profile);
                        lastMenuIdx = envMenuIdx;
                        continue;
                    }
                }
            } else { // IM_EDIT
                // UP/DOWN modify field
                if (evt == UI_EVT_UP) {
                    if (editField == 0) { editDay++;   if (editDay   > 31) editDay   = 1;  }
                    else if (editField == 1) { editMonth++; if (editMonth > 12) editMonth = 1;  }
                    else if (editField == 2) { editYear++;  if (editYear  > 2099) editYear = 2024; }
                    oled_show_incubation_day_set(0, editDay, editMonth, editYear, true, editField);
                } else if (evt == UI_EVT_DOWN) {
                    if (editField == 0) { editDay--;   if (editDay   < 1)  editDay   = 31; }
                    else if (editField == 1) { editMonth--; if (editMonth < 1)  editMonth = 12; }
                    else if (editField == 2) { editYear--;  if (editYear  < 2020) editYear = 2099; }
                    oled_show_incubation_day_set(0, editDay, editMonth, editYear, true, editField);
                } else if (evt == UI_EVT_OK) {
                    editField++;
                    if (editField < 3) {
                        oled_show_incubation_day_set(0, editDay, editMonth, editYear, true, editField);
                    } else {
                        // Final OK: save startEpoch and reset lastTurnEpoch
                        DateTime startDate(editYear, editMonth, editDay, 0, 0, 0);
                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            gSettings.startEpoch = startDate.unixtime();
                            gSettings.lastTurnEpoch = 0;
                            xSemaphoreGive(settingsMutex);
                        }
                        saveSettings();

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
                        mode = IM_NAV; navIdx = 0; lastMenuIdx = navIdx;
                        oled_show_incubation_day_set(navIdx, dispDay, dispMonth, dispYear, false, 0);
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
                continue;
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
                            uiState = UI_SET_ENV_MENU; lastMenuIdx = -1;
                            oled_show_set_environment(envMenuIdx, profile);
                            lastMenuIdx = envMenuIdx;
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
                        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            gSettings.lastTurnEpoch = 0;
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
                continue;
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
                        uiState = UI_SET_ENV_MENU; lastMenuIdx = -1;
                        oled_show_set_environment(envMenuIdx, profile);
                        lastMenuIdx = envMenuIdx;
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
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.climateMode = CLIMATE_FIXED_SCHEDULE;
                        xSemaphoreGive(settingsMutex);
                    }
                    uiState = UI_CLIMATE_SCHEDULE;
                    climateSchedIdx = 0;
                    uint8_t sh = DEFAULT_SCHED_START_HOUR, eh = DEFAULT_SCHED_END_HOUR;
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        sh = gSettings.schedStartHour;
                        eh = gSettings.schedEndHour;
                        xSemaphoreGive(settingsMutex);
                    }
                    oled_show_climate_schedule(climateSchedIdx, sh, eh);
                } else if (climateModeIdx == 1) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.climateMode = CLIMATE_CYCLIC;
                        gSettings.cycleStartEpoch = 0;  // reset on mode change
                        xSemaphoreGive(settingsMutex);
                    }
                    uiState = UI_CLIMATE_CYCLIC;
                    climateCyclicIdx = 0;
                    uint32_t hp = DEFAULT_CLIMATE_HEAT_PERIOD_MIN, cp = DEFAULT_CLIMATE_COOL_PERIOD_MIN;
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        hp = gSettings.heatPeriodMin;
                        cp = gSettings.coolPeriodMin;
                        xSemaphoreGive(settingsMutex);
                    }
                    oled_show_climate_cyclic(climateCyclicIdx, hp, cp);
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
                    uiState = UI_SET_ENV_MENU; lastMenuIdx = -1;
                    oled_show_set_environment(envMenuIdx, profile);
                    lastMenuIdx = envMenuIdx;
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // CLIMATE FIXED SCHEDULE EDIT (0=start, 1=end, 2=back)
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_CLIMATE_SCHEDULE) {
            uint8_t sh = DEFAULT_SCHED_START_HOUR, eh = DEFAULT_SCHED_END_HOUR;
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                sh = gSettings.schedStartHour;
                eh = gSettings.schedEndHour;
                xSemaphoreGive(settingsMutex);
            }

            if (climateSchedIdx == 0) {
                if (evt == UI_EVT_UP) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.schedStartHour = (gSettings.schedStartHour + 1) % 24;
                        sh = gSettings.schedStartHour;
                        xSemaphoreGive(settingsMutex);
                    }
                } else if (evt == UI_EVT_DOWN) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.schedStartHour = (gSettings.schedStartHour + 23) % 24;
                        sh = gSettings.schedStartHour;
                        xSemaphoreGive(settingsMutex);
                    }
                }
            } else if (climateSchedIdx == 1) {
                if (evt == UI_EVT_UP) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.schedEndHour = (gSettings.schedEndHour + 1) % 24;
                        eh = gSettings.schedEndHour;
                        xSemaphoreGive(settingsMutex);
                    }
                } else if (evt == UI_EVT_DOWN) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.schedEndHour = (gSettings.schedEndHour + 23) % 24;
                        eh = gSettings.schedEndHour;
                        xSemaphoreGive(settingsMutex);
                    }
                }
            }

            if (evt == UI_EVT_OK) {
                if (climateSchedIdx < 2) {
                    climateSchedIdx++;
                } else {
                    saveSettings();
                    uiState = UI_CLIMATE_MODE_MENU; lastMenuIdx = -1;
                    oled_show_climate_mode_menu(climateModeIdx, climMode);
                    lastMenuIdx = climateModeIdx;
                    continue;
                }
            }
            oled_show_climate_schedule(climateSchedIdx, sh, eh);
        }

        // ═════════════════════════════════════════════════════════════════════
        // CLIMATE CYCLIC EDIT (0=heat period, 1=cool period, 2=back)
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_CLIMATE_CYCLIC) {
            uint32_t hp = DEFAULT_CLIMATE_HEAT_PERIOD_MIN, cp = DEFAULT_CLIMATE_COOL_PERIOD_MIN;
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                hp = gSettings.heatPeriodMin;
                cp = gSettings.coolPeriodMin;
                xSemaphoreGive(settingsMutex);
            }

            if (climateCyclicIdx == 0) {
                if (evt == UI_EVT_UP) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.heatPeriodMin = min(gSettings.heatPeriodMin + 30UL, 1440UL);
                        hp = gSettings.heatPeriodMin;
                        xSemaphoreGive(settingsMutex);
                    }
                } else if (evt == UI_EVT_DOWN) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        if (gSettings.heatPeriodMin > 30) gSettings.heatPeriodMin -= 30;
                        hp = gSettings.heatPeriodMin;
                        xSemaphoreGive(settingsMutex);
                    }
                }
            } else if (climateCyclicIdx == 1) {
                if (evt == UI_EVT_UP) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        gSettings.coolPeriodMin = min(gSettings.coolPeriodMin + 30UL, 1440UL);
                        cp = gSettings.coolPeriodMin;
                        xSemaphoreGive(settingsMutex);
                    }
                } else if (evt == UI_EVT_DOWN) {
                    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                        if (gSettings.coolPeriodMin > 30) gSettings.coolPeriodMin -= 30;
                        cp = gSettings.coolPeriodMin;
                        xSemaphoreGive(settingsMutex);
                    }
                }
            }

            if (evt == UI_EVT_OK) {
                if (climateCyclicIdx < 2) {
                    climateCyclicIdx++;
                } else {
                    saveSettings();
                    uiState = UI_CLIMATE_MODE_MENU; lastMenuIdx = -1;
                    oled_show_climate_mode_menu(climateModeIdx, climMode);
                    lastMenuIdx = climateModeIdx;
                    continue;
                }
            }
            oled_show_climate_cyclic(climateCyclicIdx, hp, cp);
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

        // ═════════════════════════════════════════════════════════════════════
        // SETTINGS MENU
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_SETTINGS_MENU) {
            if      (evt == UI_EVT_UP)   settingsMenuIdx = (settingsMenuIdx - 1 + SET_COUNT) % SET_COUNT;
            else if (evt == UI_EVT_DOWN) settingsMenuIdx = (settingsMenuIdx + 1) % SET_COUNT;

            if (settingsMenuIdx != lastMenuIdx) {
                oled_show_settings_menu(settingsMenuIdx);
                lastMenuIdx = settingsMenuIdx;
            }

            if (evt == UI_EVT_OK) {
                switch ((SettingsMenuItem)settingsMenuIdx) {
                    case SET_TIME_DATE: {
                        // Initialise submenu index and open Time & Date menu
                        timeDateMenuIdx = 0;
                        uiState     = UI_TIME_DATE_MENU;
                        lastMenuIdx = -1;
                        oled_show_time_date_menu(timeDateMenuIdx);
                        lastMenuIdx = timeDateMenuIdx;
                        break;
                    }
                    case SET_WIFI:
                        uiState     = UI_WIFI_MENU;
                        wifiMenuIdx = 0;
                        lastMenuIdx = -1;
                        oled_show_wifi_menu(wifiMenuIdx, WiFi.status() == WL_CONNECTED);
                        lastMenuIdx = wifiMenuIdx;
                        break;
                    case SET_MODE:
                        uiState = UI_MODE_MENU;
                        modeMenuIdx = (ctrlMode == MODE_AUTO) ? 0 : 1;
                        lastMenuIdx = -1;
                        oled_show_mode_menu(modeMenuIdx);
                        lastMenuIdx = modeMenuIdx;
                        break;
                    case SET_DEVICE_INFO: {
                        uiState = UI_DEVICE_INFO;
                        String ip = WiFi.localIP().toString();
                        uint32_t up = millis() / 1000;
                        oled_show_device_info(DEVICE_ID, FW_VERSION, ip.c_str(),
                            profile == PROFILE_EGG_INCUBATOR ? "Incubator" : "Climate",
                            up);
                        break;
                    }
                    case SET_FACTORY_RESET:
                        uiState = UI_FACTORY_RESET_CONFIRM;
                        oled_show_factory_reset_confirm();
                        break;
                    case SET_BACK:
                        uiState = UI_MAIN_MENU; lastMenuIdx = -1;
                        oled_show_menu(mainMenuIdx);
                        lastMenuIdx = mainMenuIdx;
                        break;
                    default:
                        break;
                }
            }
        }

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
                    uiState = UI_SETTINGS_MENU;
                    lastMenuIdx = -1;
                    oled_show_settings_menu(settingsMenuIdx);
                    lastMenuIdx = settingsMenuIdx;
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
                    uiState = UI_SETTINGS_MENU; lastMenuIdx = -1;
                    oled_show_settings_menu(settingsMenuIdx);
                    lastMenuIdx = settingsMenuIdx;
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
                    uiState = UI_SETTINGS_MENU; lastMenuIdx = -1;
                    oled_show_settings_menu(settingsMenuIdx);
                    lastMenuIdx = settingsMenuIdx;
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
                uiState = UI_SETTINGS_MENU; lastMenuIdx = -1;
                oled_show_settings_menu(settingsMenuIdx);
                lastMenuIdx = settingsMenuIdx;
            }
        }

        // ═════════════════════════════════════════════════════════════════════
        // FACTORY RESET CONFIRM
        // ═════════════════════════════════════════════════════════════════════
        else if (uiState == UI_FACTORY_RESET_CONFIRM) {
            if (evt == UI_EVT_OK) {
                // Wipe NVS and restart
                extern void factoryReset(void);
                factoryReset();
            } else if (evt == UI_EVT_DOWN) {
                uiState = UI_SETTINGS_MENU; lastMenuIdx = -1;
                oled_show_settings_menu(settingsMenuIdx);
                lastMenuIdx = settingsMenuIdx;
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
                    // WIFI SYNC: attempt NTP and stay in sync-result screen
                    oled_show_time_wifi_sync(0);  // "Syncing Time..."
                    int syncResult = 2;
                    if (WiFi.status() == WL_CONNECTED) {
                        configTime(19800, 0, "pool.ntp.org");
                        struct tm timeinfo;
                        // getLocalTime is RTOS-aware (uses sntp internal semaphore)
                        if (getLocalTime(&timeinfo, 5000)) {
                            rtc.adjust(DateTime(
                                timeinfo.tm_year + 1900,
                                timeinfo.tm_mon  + 1,
                                timeinfo.tm_mday,
                                timeinfo.tm_hour,
                                timeinfo.tm_min,
                                timeinfo.tm_sec
                            ));
                            Serial.printf("[NTP] RTC updated: %04d-%02d-%02d %02d:%02d:%02d\n",
                                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                            syncResult = 1;  // success
                        } else {
                            Serial.println("[NTP] getLocalTime timed out");
                        }
                    } else {
                        Serial.println("[NTP] WiFi not connected");
                    }
                    oled_show_time_wifi_sync(syncResult);
                    // Display result for ~2 seconds using RTOS-aware delay
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    uiState = UI_SETTINGS_MENU;
                    lastMenuIdx = -1;
                    oled_show_settings_menu(settingsMenuIdx);
                    lastMenuIdx = settingsMenuIdx;

                } else {
                    // BACK
                    uiState = UI_SETTINGS_MENU;
                    lastMenuIdx = -1;
                    oled_show_settings_menu(settingsMenuIdx);
                    lastMenuIdx = settingsMenuIdx;
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
                        case 3: tD  = (tD  % 31) + 1;                        break;  // 1-31
                        case 4: tMo = (tMo % 12) + 1;                        break;  // 1-12
                        case 5: tY  = min(2099, tY + 1);                     break;
                    }
                } else if (evt == UI_EVT_DOWN) {
                    switch (tEditField) {
                        case 0: tH  = (tH  - 1 + 24) % 24;                   break;
                        case 1: tM  = (tM  - 1 + 60) % 60;                   break;
                        case 2: tS  = (tS  - 1 + 60) % 60;                   break;
                        case 3: tD  = ((tD  - 2 + 31) % 31) + 1;             break;
                        case 4: tMo = ((tMo - 2 + 12) % 12) + 1;             break;
                        case 5: tY  = max(2000, tY - 1);                     break;
                    }
                } else if (evt == UI_EVT_OK) {
                    // advance to next field
                    tEditField++;
                }
                oled_show_time_edit(tEditField, tH, tM, tS, tD, tMo, tY);

            } else {
                // field == 6: SAVE? prompt
                if (evt == UI_EVT_OK) {
                    // Write to DS1307 RTC
                    rtc.adjust(DateTime(tY, tMo, tD, tH, tM, tS));
                    Serial.printf("[RTC] Manually set to %04d-%02d-%02d %02d:%02d:%02d\n",
                                  tY, tMo, tD, tH, tM, tS);
                    uiState = UI_SETTINGS_MENU;
                    lastMenuIdx = -1;
                    oled_show_settings_menu(settingsMenuIdx);
                    lastMenuIdx = settingsMenuIdx;
                } else if (evt == UI_EVT_DOWN) {
                    // Cancel — discard edits
                    uiState = UI_SETTINGS_MENU;
                    lastMenuIdx = -1;
                    oled_show_settings_menu(settingsMenuIdx);
                    lastMenuIdx = settingsMenuIdx;
                }
                // UP has no effect on save prompt
            }
        }

        // ═════════════════════════════════════════════════════════════════
        // TIME WIFI SYNC (fallback: any button exits after NTP result is shown)
        // Note: main NTP sync is performed inline in UI_TIME_DATE_MENU handler.
        // This state is only entered if the device returns here from that flow.
        // ═════════════════════════════════════════════════════════════════
        else if (uiState == UI_TIME_WIFI_SYNC) {
            // Any button dismisses the result screen
            if (evt == UI_EVT_OK || evt == UI_EVT_DOWN || evt == UI_EVT_UP) {
                uiState = UI_SETTINGS_MENU;
                lastMenuIdx = -1;
                oled_show_settings_menu(settingsMenuIdx);
                lastMenuIdx = settingsMenuIdx;
            }
        }

    } // end for(;;)
}
