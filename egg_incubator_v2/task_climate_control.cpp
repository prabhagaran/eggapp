#include "task_control.h"
#include "globals.h"
#include "config.h"
#include "climate_logic.h"
#include <Arduino.h>
#include "esp_task_wdt.h"

// ─────────────────────────────────────────────────────────────────────────────
// TASK: CLIMATE CHAMBER CONTROL
//
// Runs every 500 ms. Implements:
//   1. Sensor validity gate
//   2. Over-temperature fault latch
//   3. Dual heater+cooler control gated by active climate sub-mode:
//      - CLIMATE_FIXED_SCHEDULE : time-window gates heat vs cool
//      - CLIMATE_CYCLIC         : alternating heat/cool periods
//      - CLIMATE_RAMP           : temperature setpoint follows ramp steps
//   4. Humidifier hysteresis (shared with incubator logic)
//   5. Manual mode override
//
// This task is SUSPENDED when PROFILE_EGG_INCUBATOR is active.
// ─────────────────────────────────────────────────────────────────────────────
void task_climate_control(void* pvParameters) {

    static unsigned long lastSensorErrorMs = 0;

    for (;;) {

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
            allRelaysOff();
            if (millis() - lastSensorErrorMs > SENSOR_ERROR_THROTTLE_MS) {
                pushError("SENSOR_ERROR", "Temp sensor invalid");
                lastSensorErrorMs = millis();
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
            allRelaysOff();
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // ── 3. Over-temperature detection ────────────────────────────────────
        if (currentTemp > MAX_TEMP_CLIMATE) {
            portENTER_CRITICAL(&faultMux);
            overTempFault = true;
            portEXIT_CRITICAL(&faultMux);

            allRelaysOff();
            pushError("OVER_TEMP", "Climate chamber over temperature");
            Serial.println("[FAULT] CLIMATE OVER TEMP — latched");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // ── Read settings ────────────────────────────────────────────────────
        float          tempSP      = DEFAULT_TEMP_SETPOINT;
        float          tempHyst    = DEFAULT_TEMP_HYSTERESIS;
        float          humSP       = DEFAULT_HUM_SETPOINT;
        float          humHyst     = DEFAULT_HUM_HYSTERESIS;
        ControlMode    ctrlMode    = MODE_AUTO;
        bool           heatMan     = false;
        bool           coolMan     = false;
        ClimateModeType climMode   = CLIMATE_FIXED_SCHEDULE;
        uint32_t       nowEpoch    = 0;
        uint8_t        currentHour = 0;

        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            tempSP   = gSettings.tempSetpoint;
            tempHyst = gSettings.tempHysteresis;
            humSP    = gSettings.humSetpoint;
            humHyst  = gSettings.humHysteresis;
            ctrlMode = gSettings.controlMode;
            heatMan  = gSettings.heaterManualOn;
            coolMan  = gSettings.coolerManualOn;
            climMode = gSettings.climateMode;
            xSemaphoreGive(settingsMutex);
        }

        if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            nowEpoch    = gRtcTime.epoch;
            currentHour = gRtcTime.now.hour();
            xSemaphoreGive(rtcMutex);
        }

        // ── 4. Control logic ─────────────────────────────────────────────────
        if (ctrlMode == MODE_MANUAL) {
            setRelay(RELAY_HEATER, heatMan);
            setRelay(RELAY_COOLER, coolMan);

        } else {

            float  targetTemp     = tempSP;
            bool   heaterAllowed  = true;
            bool   coolerAllowed  = true;

            switch (climMode) {

                case CLIMATE_FIXED_SCHEDULE:
                    heaterAllowed = scheduleAllowsHeat(currentHour);
                    coolerAllowed = !heaterAllowed;
                    break;

                case CLIMATE_CYCLIC:
                    if (cyclicInHeatPhase(nowEpoch)) {
                        heaterAllowed = true;
                        coolerAllowed = false;
                    } else {
                        heaterAllowed = false;
                        coolerAllowed = true;
                    }
                    break;

                case CLIMATE_RAMP:
                    targetTemp    = getRampTargetTemp(nowEpoch);
                    heaterAllowed = true;
                    coolerAllowed = true;
                    break;
            }

            applyDualControl(currentTemp, targetTemp, tempHyst, heaterAllowed, coolerAllowed);
        }

        // ── 5. Humidifier control ────────────────────────────────────────────
        bool humOn = false;
        if (xSemaphoreTake(controlMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            humOn = gRelayState.humidifierOn;
            xSemaphoreGive(controlMutex);
        }

        if (!humOn && currentHum <= (humSP - humHyst)) {
            setRelay(RELAY_HUMIDIFIER, true);
        } else if (humOn && currentHum >= (humSP + humHyst)) {
            setRelay(RELAY_HUMIDIFIER, false);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
