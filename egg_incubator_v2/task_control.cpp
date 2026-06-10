#include "task_control.h"
#include "globals.h"
#include "config.h"
#include <Arduino.h>
#include "esp_task_wdt.h"

// ─────────────────────────────────────────────────────────────────────────────
// TASK: TEMPERATURE CONTROL (Egg Incubator profile)
//
// Runs every 500 ms. Implements:
//   1. Sensor validity gate — all outputs OFF on invalid reading
//   2. Over-temperature fault latch — all outputs OFF, latches until reset
//   3. Hysteresis heater control (heater only, no cooler in incubator mode)
//   4. Hysteresis humidifier control
//   5. Manual mode override (heaterManualOn drives relay directly)
//
// This task is SUSPENDED when PROFILE_CLIMATE_CHAMBER is active.
// ─────────────────────────────────────────────────────────────────────────────
void task_temperature_control(void* pvParameters) {

    // WDT subscription is done by setup() via esp_task_wdt_add(hTaskTempControl)
    // AFTER the initial profile-based vTaskSuspend calls, so we never subscribe
    // while frozen (a suspended task cannot call reset()).

    static unsigned long lastSensorErrorMs = 0;

    for (;;) {

        esp_task_wdt_reset();  // pet the watchdog at the top of every cycle

        // ── Safe self-suspend point (profile switch via task notification) ────
        // Called at loop top: no mutex is held here.
        { uint32_t cmd = 0;
          if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &cmd, 0) == pdTRUE && cmd == TASK_CMD_SUSPEND) {
              xEventGroupSetBits(suspendAckGroup, TASK_SUSPEND_BIT_TEMP_CTRL);
              esp_task_wdt_delete(NULL);  // unsubscribe: a suspended task cannot reset
              vTaskSuspend(NULL);
              // ── resumed here by switchProfile() ──────────────────────────
              esp_task_wdt_add(NULL);    // re-subscribe now that we are running again
          }
        }

        // ── Read sensor data ─────────────────────────────────────────────────
        float currentTemp = -999.0f;
        float currentHum  = 0.0f;
        bool  tempValid   = false;

        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            currentTemp = gSensorData.temp_ds18b20;
            currentHum  = gSensorData.humidity_dht;
            tempValid   = gSensorData.temp_valid;
            xSemaphoreGive(sensorMutex);
        }

        // ── 1. Sensor validity gate ──────────────────────────────────────────
        if (!tempValid) {
            setRelay(RELAY_HEATER,     false);
            setRelay(RELAY_HUMIDIFIER, false);

            if (millis() - lastSensorErrorMs > SENSOR_ERROR_THROTTLE_MS) {
                pushError("SENSOR_ERROR", "Temp sensor invalid");
                lastSensorErrorMs = millis();
                Serial.println("[SAFETY] Sensor invalid — outputs OFF");
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // ── 2. Fault latch gate ──────────────────────────────────────────────
        bool fault = false;
        portENTER_CRITICAL(&faultMux);
        fault = overTempFault;
        portEXIT_CRITICAL(&faultMux);

        if (fault) {
            setRelay(RELAY_HEATER,     false);
            setRelay(RELAY_HUMIDIFIER, false);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // ── 3. Over-temperature detection ────────────────────────────────────
        if (currentTemp > MAX_TEMP_INCUBATOR) {
            portENTER_CRITICAL(&faultMux);
            overTempFault = true;
            portEXIT_CRITICAL(&faultMux);

            setRelay(RELAY_HEATER,     false);
            setRelay(RELAY_HUMIDIFIER, false);

            pushError("OVER_TEMP", "Incubator over temperature");
            Serial.println("[FAULT] OVER TEMPERATURE — latched");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // ── Read settings ────────────────────────────────────────────────────
        float      tempSP    = DEFAULT_TEMP_SETPOINT;
        float      tempHyst  = DEFAULT_TEMP_HYSTERESIS;
        float      humSP     = DEFAULT_HUM_SETPOINT;
        float      humHyst   = DEFAULT_HUM_HYSTERESIS;
        ControlMode ctrlMode = MODE_AUTO;
        bool       heatMan   = false;

        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            tempSP   = gSettings.tempSetpoint;
            tempHyst = gSettings.tempHysteresis;
            humSP    = gSettings.humSetpoint;
            humHyst  = gSettings.humHysteresis;
            ctrlMode = gSettings.controlMode;
            heatMan  = gSettings.heaterManualOn;
            xSemaphoreGive(settingsMutex);
        }

        // ── 4. Control logic ─────────────────────────────────────────────────
        if (ctrlMode == MODE_MANUAL) {

            // Manual mode: relay directly follows user toggle
            setRelay(RELAY_HEATER, heatMan);
            // Cooler is not used in egg incubator profile

        } else {

            // Auto mode: hysteresis heater control
            bool heaterOn = false;
            if (xSemaphoreTake(controlMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                heaterOn = gRelayState.heaterOn;
                xSemaphoreGive(controlMutex);
            }

            if (!heaterOn && currentTemp <= (tempSP - tempHyst)) {
                setRelay(RELAY_HEATER, true);
                Serial.printf("[CTRL] Heater ON  (%.1f / %.1f)\n", currentTemp, tempSP);
            } else if (heaterOn && currentTemp >= (tempSP + tempHyst)) {
                setRelay(RELAY_HEATER, false);
                Serial.printf("[CTRL] Heater OFF (%.1f / %.1f)\n", currentTemp, tempSP);
            }
        }

        // ── 5. Humidifier hysteresis control (same in auto & manual) ─────────
        bool humOn = false;
        if (xSemaphoreTake(controlMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            humOn = gRelayState.humidifierOn;
            xSemaphoreGive(controlMutex);
        }

        if (!humOn && currentHum <= (humSP - humHyst)) {
            setRelay(RELAY_HUMIDIFIER, true);
            Serial.printf("[CTRL] Humidifier ON  (%.0f / %.0f)\n", currentHum, humSP);
        } else if (humOn && currentHum >= (humSP + humHyst)) {
            setRelay(RELAY_HUMIDIFIER, false);
            Serial.printf("[CTRL] Humidifier OFF (%.0f / %.0f)\n", currentHum, humSP);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
