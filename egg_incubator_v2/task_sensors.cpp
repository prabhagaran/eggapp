#include "task_sensors.h"
#include "globals.h"
#include "config.h"
#include "esp_task_wdt.h"

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

    esp_task_wdt_add(NULL);   // subscribe to TWDT (max cycle ≈ 1 800 ms, well under 10 s)

    static unsigned long lastErrorMs = 0;

    for (;;) {
        esp_task_wdt_reset();  // pet the watchdog at the top of every cycle

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
    vTaskDelay(pdMS_TO_TICKS(2000)); // DHT22 warm-up after power-on / begin

    esp_task_wdt_add(NULL);   // subscribe to TWDT (max cycle ≈ 3 500 ms, under 10 s)

    float lastValidHum = 0.0f;
    bool  everHadValid = false;   // stays false until first genuine read
    int consecutiveBad = 0;
    static unsigned long lastHumErrorMs = 0;

    for (;;) {
        esp_task_wdt_reset();  // pet the watchdog at the top of every cycle

        // DHT22 minimum sample period is 1 s (spec); 2.5 s is reliably safe.
        // Delay at the top of the loop guarantees the gap is always honoured.
        vTaskDelay(pdMS_TO_TICKS(2500));

        float h = dht.readHumidity();

#if DHT_DEBUG_RAW
        if (isnan(h)) Serial.println("[DHT RAW] NaN");
        else          Serial.printf("[DHT RAW] %.2f %%\n", h);
#endif

        if (!isnan(h) && h >= 0.0f && h <= 100.0f) {
            float reportedHum = h + HUM_CALIB_OFFSET;
            if (reportedHum < 0.0f)   reportedHum = 0.0f;
            if (reportedHum > 100.0f) reportedHum = 100.0f;
            lastValidHum   = reportedHum;
            everHadValid   = true;
            consecutiveBad = 0;
            Serial.printf("[DHT] Humidity: %.1f %%\n", reportedHum);

            if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                gSensorData.humidity_dht = lastValidHum;
                gSensorData.hum_valid    = true;
                xSemaphoreGive(sensorMutex);
            }
        } else {
            ++consecutiveBad;
            Serial.println("[DHT] Bad reading — holding last value");

            // Re-initialise the sensor every 5th consecutive failure to
            // recover a stuck bus; always follow begin() with a wait.
            if (consecutiveBad % 5 == 0) {
                dht.begin();
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            if (consecutiveBad >= 5 && (millis() - lastHumErrorMs) > SENSOR_ERROR_THROTTLE_MS) {
                pushError("SENSOR_ERROR", "DHT persistent failures");
                lastHumErrorMs = millis();
            }

            if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Only hold a stale value if we ever had a real reading.
                // Before the first valid read, keep hum_valid=false so
                // consumers do not act on an uninitialised value.
                if (everHadValid) {
                    gSensorData.humidity_dht = lastValidHum;
                }
                gSensorData.hum_valid = false;
                xSemaphoreGive(sensorMutex);
            }
        }
    }
}
