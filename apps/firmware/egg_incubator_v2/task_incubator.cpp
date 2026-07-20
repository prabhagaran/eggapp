#include "task_incubator.h"
#include "globals.h"
#include "config.h"
#include "incubator_logic.h"
#include <Arduino.h>
#include <Preferences.h>
// ESP32 Arduino LEDC helpers and IDF driver
#include <esp32-hal-ledc.h>
#include <driver/ledc.h>

// ─────────────────────────────────────────────────────────────────────────────
// TASK: EGG TURNER
//
// Checks on each iteration if turnerIntervalMin has elapsed since lastTurnEpoch.
// If yes, runs the motor for turnerDurationSec, then records the new lastTurnEpoch.
// Auto-stops if past lockdown day (turnerActive() returns false).
// Suspended when climate chamber profile is active.
// ─────────────────────────────────────────────────────────────────────────────
void task_turner(void* pvParameters) {

    for (;;) {

        // ── Safe self-suspend point (profile switch via task notification) ────
        { uint32_t cmd = 0;
          if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &cmd, 0) == pdTRUE && cmd == TASK_CMD_SUSPEND) {
              xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_TURNER);
              vTaskSuspend(NULL);
          }
        }

        // ── Remote manual override: skip auto scheduling, drive relay directly ──
        {
            bool ovr = false, on = false;
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                ovr = gSettings.turnerManualOverride;
                on  = gSettings.turnerManualOn;
                xSemaphoreGive(settingsMutex);
            }
            if (ovr) {
                setRelay(RELAY_TURNER, on);
                uint32_t icmd = 0;
                if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &icmd, pdMS_TO_TICKS(2000)) == pdTRUE && icmd == TASK_CMD_SUSPEND) {
                    setRelay(RELAY_TURNER, false);
                    xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_TURNER);
                    vTaskSuspend(NULL);
                }
                continue;
            }
        }

        if (!turnerActive()) {
            // Past lockdown — keep relay off and wait
            setRelay(RELAY_TURNER, false);
            { uint32_t icmd = 0;
              if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &icmd, pdMS_TO_TICKS(60000)) == pdTRUE && icmd == TASK_CMD_SUSPEND) {
                  xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_TURNER);
                  vTaskSuspend(NULL);
              }
            }
            continue;
        }

        uint32_t lastTurn     = 0;
        uint32_t intervalSec  = 0;
        uint32_t durationSec  = 0;
        uint32_t nowEpoch     = 0;

        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            lastTurn    = gSettings.lastTurnEpoch;
            intervalSec = (uint32_t)gSettings.turnerIntervalMin * 60UL;
            durationSec = (uint32_t)gSettings.turnerDurationSec;
            xSemaphoreGive(settingsMutex);
        }

        if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            nowEpoch = gRtcTime.epoch;
            xSemaphoreGive(rtcMutex);
        }

        // Epoch must be sane before any subtraction; DS1307 reset returns year 2000.
        if (!rtcEpochValid) {
            { uint32_t icmd = 0;
              if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &icmd, pdMS_TO_TICKS(10000)) == pdTRUE && icmd == TASK_CMD_SUSPEND) {
                  xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_TURNER);
                  vTaskSuspend(NULL);
              }
            }
            continue;
        }

        // Bootstrap: if never turned before, record now and wait full interval
        if (lastTurn == 0) {
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                gSettings.lastTurnEpoch = nowEpoch;
                xSemaphoreGive(settingsMutex);
            }
            // Persist initial lastTurnEpoch so a reboot doesn't re-run an early turn
            {
                Preferences prefs;
                prefs.begin("incubator", false);
                prefs.putULong("lastTurn", nowEpoch);
                prefs.end();
            }
            { uint32_t icmd = 0;
              if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &icmd, pdMS_TO_TICKS(30000)) == pdTRUE && icmd == TASK_CMD_SUSPEND) {
                  xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_TURNER);
                  vTaskSuspend(NULL);
              }
            }
            continue;
        }

        if ((nowEpoch - lastTurn) >= intervalSec) {

            Serial.printf("[TURNER] Turning eggs for %lu seconds\n", durationSec);
            setRelay(RELAY_TURNER, true);

            { uint32_t icmd = 0;
              if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &icmd, pdMS_TO_TICKS(durationSec * 1000UL)) == pdTRUE && icmd == TASK_CMD_SUSPEND) {
                  // Profile switch mid-turn — stop motor and suspend immediately
                  setRelay(RELAY_TURNER, false);
                  xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_TURNER);
                  vTaskSuspend(NULL);
                  continue;  // restart loop on resume; lastTurnEpoch left unchanged so turn will re-run
              }
            }

            setRelay(RELAY_TURNER, false);

            // Re-read RTC now that the turn completed, and save new lastTurnEpoch
            {
                uint32_t finishedEpoch = 0;
                if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    finishedEpoch = gRtcTime.epoch;
                    xSemaphoreGive(rtcMutex);
                }

                if (finishedEpoch == 0) {
                    // Fallback to previous value if RTC not available
                    finishedEpoch = nowEpoch;
                }

                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    gSettings.lastTurnEpoch = finishedEpoch;
                    xSemaphoreGive(settingsMutex);
                }

                // Persist only this single key to NVS to avoid writing all settings here
                {
                    Preferences prefs;
                    prefs.begin("incubator", false);
                    prefs.putULong("lastTurn", finishedEpoch);
                    prefs.end();
                }
            }

            // Also persist to NVS via main saveSettings — signal via a flag
            // (saveSettings is called from main .ino; we don't call it here to
            //  avoid double-writing all keys. Instead lastTurnEpoch is saved by
            //  the UI task's periodic NVS sync, or on next confirmed UI save.)
            Serial.println("[TURNER] Done");
        }

        { uint32_t icmd = 0;
          if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &icmd, pdMS_TO_TICKS(10000)) == pdTRUE && icmd == TASK_CMD_SUSPEND) {
              xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_TURNER);
              vTaskSuspend(NULL);
          }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TASK: EXHAUST FAN
//
// Runs fan using PWM speed from `gSettings.fanSpeedPercent`.
// Ensures fan is disabled when not in the incubator profile.
// ─────────────────────────────────────────────────────────────────────────────
void task_fan(void* pvParameters) {

    // Non-blocking PWM-based fan control using LEDC (8-bit)
    // Periodically reads `gSettings.fanSpeedPercent` and `gSettings.activeProfile`
    // and applies PWM. Does not block for on-duration.
    for (;;) {
        // ── Safe self-suspend point (profile switch via task notification) ────
        { uint32_t cmd = 0;
          if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &cmd, 0) == pdTRUE && cmd == TASK_CMD_SUSPEND) {
              xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_FAN);
              vTaskSuspend(NULL);
          }
        }

        uint8_t speed = 0;
        ProfileType profile = PROFILE_EGG_INCUBATOR;
        bool fanOverride = false, fanOn = false;

        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            speed = (uint8_t)gSettings.fanSpeedPercent;
            profile = gSettings.activeProfile;
            fanOverride = gSettings.fanManualOverride;
            fanOn = gSettings.fanManualOn;
            xSemaphoreGive(settingsMutex);
        }

        if (profile != PROFILE_EGG_INCUBATOR) {
            // Ensure fan is off when not in incubator profile
            setFanSpeed(0);
        } else if (fanOverride) {
            // Remote manual override: full speed or off, bypassing fanSpeedPercent
            setFanSpeed(fanOn ? 100 : 0);
        } else {
            setFanSpeed(speed);
        }

        // Sleep briefly and re-check settings; not blocking the fan run
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// One-time LEDC init — MUST be called from setup() before any task starts.
// Was previously a lazy init inside setFanSpeed() guarded by a non-atomic
// static flag, racy when first called concurrently from UI/control/fan tasks.
void initFanPwm(void) {
    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)LEDC_TIMER_8_BIT,  // 0..255
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 25000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&tcfg);

    ledc_channel_config_t chcfg = {
        .gpio_num = RELAY_FAN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 255,  // active-LOW: start HIGH = relay OFF until task_fan applies speed
        .hpoint = 0
    };
    ledc_channel_config(&chcfg);
}

// LEDC-based fan PWM helper (uses ESP-IDF driver API for portability)
void setFanSpeed(uint8_t percent) {
    const int channel = 0; // LEDC channel

    if (percent > 100) percent = 100;
    // Active-LOW relay: duty=0 (GPIO LOW) = relay ON = fan running.
    // Invert so that percent=100 drives the relay fully ON and percent=0 turns it OFF.
    uint32_t duty = ((uint32_t)(100U - percent) * 255U) / 100U;
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)channel, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)channel);

    // Mirror PWM-on state into gRelayState.fanOn under controlMutex
    if (xSemaphoreTake(controlMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        gRelayState.fanOn = (percent > 0);
        xSemaphoreGive(controlMutex);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TASK: WATER PUMP
//
// Monitors humidity. Triggers pump run when humidity is well below target,
// suggesting the reservoir needs refilling. Enforces a cooldown between runs
// to prevent over-filling.
// Suspended when climate chamber profile is active.
// ─────────────────────────────────────────────────────────────────────────────
void task_pump(void* pvParameters) {

    unsigned long lastPumpMs = 0;

    for (;;) {
        // ── Safe self-suspend point (profile switch via task notification) ────
        { uint32_t cmd = 0;
          if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &cmd, 0) == pdTRUE && cmd == TASK_CMD_SUSPEND) {
              xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_PUMP);
              vTaskSuspend(NULL);
          }
        }

        // ── Remote manual override: skip auto trigger logic, drive relay directly ──
        {
            bool ovr = false, on = false;
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                ovr = gSettings.pumpManualOverride;
                on  = gSettings.pumpManualOn;
                xSemaphoreGive(settingsMutex);
            }
            if (ovr) {
                setRelay(RELAY_PUMP, on);
                uint32_t icmd = 0;
                if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &icmd, pdMS_TO_TICKS(2000)) == pdTRUE && icmd == TASK_CMD_SUSPEND) {
                    setRelay(RELAY_PUMP, false);
                    xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_PUMP);
                    vTaskSuspend(NULL);
                }
                continue;
            }
        }

        float currentHum = 0.0f;
        bool  humValid   = false;
        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            currentHum = gSensorData.humidity_dht;
            humValid   = gSensorData.hum_valid;
            xSemaphoreGive(sensorMutex);
        }

        // If humidity reading is not valid, skip pump logic to avoid acting on stale data
        if (!humValid) {
            { uint32_t icmd = 0;
              if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &icmd, pdMS_TO_TICKS(30000)) == pdTRUE && icmd == TASK_CMD_SUSPEND) {
                  xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_PUMP);
                  vTaskSuspend(NULL);
              }
            }
            continue;
        }

        float    humSP       = DEFAULT_HUM_SETPOINT;
        float    humHyst     = DEFAULT_HUM_HYSTERESIS;
        uint16_t pumpDurSec  = DEFAULT_PUMP_DURATION_SEC;

        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            humSP      = gSettings.humSetpoint;
            humHyst    = gSettings.humHysteresis;
            pumpDurSec = gSettings.pumpDurationSec;
            xSemaphoreGive(settingsMutex);
        }

        float triggerThreshold = humSP - humHyst - PUMP_TRIGGER_EXTRA_DEFICIT;
        bool  cooldownOk       = (millis() - lastPumpMs) > ((unsigned long)PUMP_COOLDOWN_SEC * 1000UL);

        if (currentHum < triggerThreshold && cooldownOk) {

            Serial.printf("[PUMP] Low reservoir likely — running pump for %d s\n", pumpDurSec);
            setRelay(RELAY_PUMP, true);

            { uint32_t icmd = 0;
              if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &icmd, pdMS_TO_TICKS((uint32_t)pumpDurSec * 1000UL)) == pdTRUE && icmd == TASK_CMD_SUSPEND) {
                  // Profile switch mid-pump — stop pump and suspend immediately
                  setRelay(RELAY_PUMP, false);
                  xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_PUMP);
                  vTaskSuspend(NULL);
                  continue;
              }
            }

            setRelay(RELAY_PUMP, false);
            lastPumpMs = millis();
            Serial.println("[PUMP] Done");
        }

        { uint32_t icmd = 0;
          if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &icmd, pdMS_TO_TICKS(30000)) == pdTRUE && icmd == TASK_CMD_SUSPEND) {
              xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_PUMP);
              vTaskSuspend(NULL);
          }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TASK: MILESTONE MONITOR
//
// Checks once per minute if today is a milestone day (candling / lockdown / hatch).
// On lockdown day: suspends turner task, raises humidity setpoint.
// On any milestone: pushes cloud alert and sets oledMilestoneLabel for home screen.
// Suspended when climate chamber profile is active.
// ─────────────────────────────────────────────────────────────────────────────

// Shared milestone label read by task_ui for home screen banner
char gMilestoneLabel[24] = { 0 };
SemaphoreHandle_t milestoneMutex = nullptr;

void task_milestone(void* pvParameters) {

    uint32_t lastCheckEpoch    = 0;
    uint32_t lastStartEpoch    = 0;   // detect new batch
    uint32_t lastMilestoneEpoch = 0;  // throttle cloud alerts (moved to task scope for batch reset)
    bool     lockdownApplied   = false;

    for (;;) {
        // ── Safe self-suspend point (profile switch via task notification) ────
        { uint32_t cmd = 0;
          if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &cmd, 0) == pdTRUE && cmd == TASK_CMD_SUSPEND) {
              xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_MILESTONE);
              vTaskSuspend(NULL);
          }
        }

        // ── Detect new batch (startEpoch changed) and reset lockdown state ───
        {
            uint32_t curStart = 0;
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                curStart = gSettings.startEpoch;
                xSemaphoreGive(settingsMutex);
            }
            if (curStart != 0 && curStart != lastStartEpoch) {
                lastStartEpoch       = curStart;
                lockdownApplied      = false;
                lastCheckEpoch       = 0;   // re-check milestone soon
                lastMilestoneEpoch   = 0;   // allow milestone alerts for new batch
                Serial.println("[MILESTONE] New batch detected — lockdown state reset");
            }
        }

        uint32_t nowEpoch = 0;
        if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            nowEpoch = gRtcTime.epoch;
            xSemaphoreGive(rtcMutex);
        }

        // Check once per minute (not every iteration); skip entirely if epoch is unreliable.
        if (nowEpoch == 0 || !rtcEpochValid || (nowEpoch - lastCheckEpoch) < 60) {
            vTaskDelay(pdMS_TO_TICKS(30000));
            continue;
        }
        lastCheckEpoch = nowEpoch;

        char label[24] = { 0 };
        bool isMilestone = checkMilestone(nowEpoch, label, sizeof(label));

        if (isMilestone) {

            // Update shared label for home screen
            if (xSemaphoreTake(milestoneMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                strncpy(gMilestoneLabel, label, sizeof(gMilestoneLabel) - 1);
                xSemaphoreGive(milestoneMutex);
            }

            // Push cloud alert (throttle: won't re-push same milestone for 23 hours)
            if ((nowEpoch - lastMilestoneEpoch) > (23UL * 3600UL)) {
                pushError("MILESTONE", label);
                lastMilestoneEpoch = nowEpoch;
                Serial.printf("[MILESTONE] %s\n", label);
            }

            // Lockdown: suspend turner, raise humidity
            if (!lockdownApplied && strstr(label, "LOCKDOWN") != nullptr) {
                lockdownApplied = true;

                if (hTaskTurner != nullptr) {
                    vTaskSuspend(hTaskTurner);
                    setRelay(RELAY_TURNER, false);
                    Serial.println("[MILESTONE] Turner suspended for lockdown");
                }

                float humToSave = 0.0f;
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    gSettings.humSetpoint = gSettings.lockdownHumidity;
                    humToSave = gSettings.humSetpoint;  // snapshot before release
                    xSemaphoreGive(settingsMutex);
                }
                // Persist only the humidity setpoint to NVS so lockdown survives power cycles
                {
                    Preferences prefs;
                    prefs.begin("incubator", false);
                    prefs.putFloat("setHum", humToSave);
                    prefs.end();
                }
                Serial.println("[MILESTONE] Humidity raised for lockdown");
            }

        } else {
            // Clear milestone label if no active milestone
            if (xSemaphoreTake(milestoneMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                gMilestoneLabel[0] = '\0';
                xSemaphoreGive(milestoneMutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
