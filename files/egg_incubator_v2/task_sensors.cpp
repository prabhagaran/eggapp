#include "task_sensors.h"
#include "globals.h"
#include "config.h"

#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

static OneWire          oneWire(DS18B20_PIN);
static DallasTemperature ds18b20(&oneWire);
static DHT              dht(DHT_PIN, DHT_TYPE);

// ─────────────────────────────────────────────────────────────────────────────
// DS18B20 TEMPERATURE TASK
// Polls every ~1.8 s (800 ms conversion + 1000 ms delay).
// Writes temp_ds18b20 and temp_valid under sensorMutex.
// Pushes error once per SENSOR_ERROR_THROTTLE_MS on disconnect.
// ─────────────────────────────────────────────────────────────────────────────
void task_ds18b20(void* pvParameters) {
    ds18b20.begin();
    ds18b20.setWaitForConversion(false);  // non-blocking conversion request

    static unsigned long lastErrorMs = 0;

    for (;;) {
        // Request conversion
        ds18b20.requestTemperatures();
        vTaskDelay(pdMS_TO_TICKS(800));  // DS18B20 needs ~750 ms for 12-bit

        float t = ds18b20.getTempCByIndex(0);

        if (t == DEVICE_DISCONNECTED_C || t == 85.0f) {
            // 85.0 is the DS18B20 power-on reset value — treat as invalid
            if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                gSensorData.temp_ds18b20 = -999.0f;
                gSensorData.temp_valid   = false;
                xSemaphoreGive(sensorMutex);
            }
            if (millis() - lastErrorMs > SENSOR_ERROR_THROTTLE_MS) {
                pushError("SENSOR_ERROR", "DS18B20 disconnected");
                lastErrorMs = millis();
                Serial.println("[DS18B20] Disconnected");
            }

        } else if (t < SENSOR_INVALID_LOW || t > SENSOR_INVALID_HIGH) {
            // Reading out of plausible range
            if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                gSensorData.temp_ds18b20 = -999.0f;
                gSensorData.temp_valid   = false;
                xSemaphoreGive(sensorMutex);
            }
            if (millis() - lastErrorMs > SENSOR_ERROR_THROTTLE_MS) {
                pushError("SENSOR_ERROR", "DS18B20 out of range");
                lastErrorMs = millis();
            }

        } else {
            if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                gSensorData.temp_ds18b20 = t;
                gSensorData.temp_valid   = true;
                xSemaphoreGive(sensorMutex);
            }
            Serial.printf("[DS18B20] Temp: %.2f C\n", t);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DHT11 HUMIDITY TASK
// Polls every 2 s. On NaN, retains last valid humidity (stale-hold strategy).
// Writes humidity_dht and hum_valid under sensorMutex.
// ─────────────────────────────────────────────────────────────────────────────
void task_sensor(void* pvParameters) {
    dht.begin();

    float lastValidHum = 50.0f;  // safe default until first valid reading

    for (;;) {
        float h = dht.readHumidity();

        if (isnan(h) || h < 0.0f || h > 100.0f) {
            // Stale hold — log but don't crash control
            Serial.println("[DHT11] Bad reading — holding last value");
        } else {
            lastValidHum = h;
            Serial.printf("[DHT11] Humidity: %.1f %%\n", h);
        }

        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            gSensorData.humidity_dht = lastValidHum;
            gSensorData.hum_valid    = !isnan(h);
            xSemaphoreGive(sensorMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
