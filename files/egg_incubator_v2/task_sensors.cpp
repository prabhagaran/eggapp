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
// DHT HUMIDITY TASK
// Polls regularly. Performs several quick attempts per cycle, averages valid
// samples, applies optional calibration offset, and uses stale-hold on failure.
// Writes humidity_dht and hum_valid under sensorMutex. Pushes a SENSOR_ERROR
// if failures persist.
// ─────────────────────────────────────────────────────────────────────────────
void task_sensor(void* pvParameters) {
    dht.begin();

    float lastValidHum = 50.0f;  // safe default until first valid reading
    int consecutiveBad = 0;
    static unsigned long lastHumErrorMs = 0;

    const int READ_ATTEMPTS = 3;
    const int READ_DELAY_MS = 200; // ms between quick attempts
    const int CYCLE_MS = 2000;     // target cycle period

    for (;;) {
        float sum = 0.0f;
        int count = 0;

        for (int i = 0; i < READ_ATTEMPTS; ++i) {
            float h = dht.readHumidity();
#if DHT_DEBUG_RAW
            if (isnan(h)) {
                Serial.printf("[DHT RAW] attempt %d: NaN\n", i + 1);
            } else {
                Serial.printf("[DHT RAW] attempt %d: %.2f %%\n", i + 1, h);
            }
#endif
            if (!isnan(h) && h >= 0.0f && h <= 100.0f) {
                sum += h;
                ++count;
            }
            vTaskDelay(pdMS_TO_TICKS(READ_DELAY_MS));
        }

        bool valid = false;
        float reportedHum = lastValidHum;

        if (count > 0) {
            reportedHum = (sum / count) + HUM_CALIB_OFFSET;
            if (reportedHum < 0.0f) reportedHum = 0.0f;
            if (reportedHum > 100.0f) reportedHum = 100.0f;
            lastValidHum = reportedHum;
            valid = true;
            consecutiveBad = 0;
            Serial.printf("[DHT] Humidity: %.1f %% (avg of %d)\n", reportedHum, count);
        } else {
            ++consecutiveBad;
            Serial.println("[DHT] Bad reading — holding last value");
            if (consecutiveBad >= 5 && (millis() - lastHumErrorMs) > SENSOR_ERROR_THROTTLE_MS) {
                pushError("SENSOR_ERROR", "DHT persistent failures");
                lastHumErrorMs = millis();
            }
        }

        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            gSensorData.humidity_dht = lastValidHum;
            gSensorData.hum_valid    = valid;
            xSemaphoreGive(sensorMutex);
        }

        int extraDelay = CYCLE_MS - (READ_ATTEMPTS * READ_DELAY_MS);
        if (extraDelay < 0) extraDelay = 0;
        vTaskDelay(pdMS_TO_TICKS(extraDelay));
    }
}
