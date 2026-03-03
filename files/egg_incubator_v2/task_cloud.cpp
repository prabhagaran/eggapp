#include "task_cloud.h"
#include "globals.h"
#include "config.h"
#include "incubator_logic.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// URL-encode a string (replaces all non-alphanumeric chars with %XX)
// ─────────────────────────────────────────────────────────────────────────────
static String urlEncode(const String& src) {
    String encoded = "";
    encoded.reserve(src.length() * 3);
    for (size_t i = 0; i < src.length(); i++) {
        char c = src[i];
        if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (uint8_t)c);
            encoded += buf;
        }
    }
    return encoded;
}

// ─────────────────────────────────────────────────────────────────────────────
// TASK: CLOUD TELEMETRY (runs on Core 0 at low priority)
//
// Every 60 s: sends a full telemetry row to Google Sheets.
// Before telemetry: drains errorQueue and sends each error.
// Every 5 min: sends heartbeat via error channel.
// Graceful WiFi reconnect with non-blocking retry.
// ─────────────────────────────────────────────────────────────────────────────
void task_cloud(void* pvParameters) {

    static unsigned long lastHeartbeatMs  = 0;
    static unsigned long lastHttpErrorMs  = 0;

    for (;;) {

        // ── WiFi check ───────────────────────────────────────────────────────
        // Only proceed if user has enabled Wi-Fi; never auto-reconnect otherwise.
        if (!wifiUserEnabled) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[CLOUD] WiFi disconnected, reconnecting...");
            WiFi.reconnect();
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // ── Heartbeat ────────────────────────────────────────────────────────
        if (millis() - lastHeartbeatMs > CLOUD_HEARTBEAT_INTERVAL_MS) {
            pushError("HEARTBEAT", "Device Alive");
            lastHeartbeatMs = millis();
        }

        // ── Drain error queue ────────────────────────────────────────────────
        ErrorMsg_t incomingErr;
        while (xQueueReceive(errorQueue, &incomingErr, 0) == pdTRUE) {

            // Build URL for this error
            String url = String(GOOGLE_SCRIPT_URL)
                + "?id="    + String(DEVICE_ID)
                + "&fw="    + String(FW_VERSION)
                + "&error=" + urlEncode(String(incomingErr.type))
                + "&msg="   + urlEncode(String(incomingErr.message));

            if (strlen(CLOUD_TOKEN) > 0) {
                url += "&token=" + urlEncode(String(CLOUD_TOKEN));
            }

            Serial.printf("[CLOUD] Error log: %s — %s\n", incomingErr.type, incomingErr.message);

            // send with retries/backoff and detailed logging
            auto sendWithRetries = [&](const String &reqUrl) -> int {
                for (int attempt = 0; attempt < CLOUD_HTTP_MAX_RETRIES; ++attempt) {
                    WiFiClientSecure client;
                    client.setInsecure();

                    HTTPClient http;
                    http.setTimeout(10000);
                    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                    http.begin(client, reqUrl);

                    Serial.printf("[CLOUD] Request: %s (attempt %d)\n", reqUrl.c_str(), attempt+1);
                    int code = http.GET();

                    String body = "";
                    if (code > 0) body = http.getString();

                    if (code > 0) {
                        Serial.printf("[CLOUD] Response code: %d\n", code);
                        if (body.length() > 0) Serial.printf("[CLOUD] Body: %s\n", body.c_str());
                        http.end();
                        return code;
                    } else {
                        Serial.printf("[CLOUD] HTTP failed: %s\n", http.errorToString(code).c_str());
                        http.end();
                        // backoff before next attempt
                        uint32_t backoff = CLOUD_HTTP_BACKOFF_MS * (1UL << attempt);
                        vTaskDelay(pdMS_TO_TICKS(backoff));
                    }
                }
                return -1;
            };

            int code = sendWithRetries(url);
            if (code <= 0) {
                Serial.printf("[CLOUD] Error send failed after retries: %d\n", code);
            }

            vTaskDelay(pdMS_TO_TICKS(500));  // brief gap between error sends
        }

        // ── Snapshot all telemetry data ──────────────────────────────────────
        float temp = 0.0f, hum = 0.0f;
        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            temp = gSensorData.temp_ds18b20;
            hum  = gSensorData.humidity_dht;
            xSemaphoreGive(sensorMutex);
        }

        float    tempSP = DEFAULT_TEMP_SETPOINT, humSP = DEFAULT_HUM_SETPOINT;
        ControlMode   ctrlMode = MODE_AUTO;
        ProfileType   prof     = PROFILE_EGG_INCUBATOR;
        ClimateModeType climMode = CLIMATE_FIXED_SCHEDULE;

        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            tempSP   = gSettings.tempSetpoint;
            humSP    = gSettings.humSetpoint;
            ctrlMode = gSettings.controlMode;
            prof     = gSettings.activeProfile;
            climMode = gSettings.climateMode;
            xSemaphoreGive(settingsMutex);
        }

        RelayState_t rs = {};
        if (xSemaphoreTake(controlMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            rs = gRelayState;
            xSemaphoreGive(controlMutex);
        }

        // ── Validate sensor values ───────────────────────────────────────────
        String tempStr = (temp < -100.0f || temp > 100.0f) ? "NA" : String(temp, 1);
        String humStr  = (hum < 0.0f    || hum  > 100.0f) ? "NA" : String((int)hum);

        // ── Incubation-specific data ─────────────────────────────────────────
        int      day      = 0;
        int      daysLeft = -1;
        uint32_t hatchEpoch = 0;

        if (prof == PROFILE_EGG_INCUBATOR) {
            day       = calcIncubationDay();
            daysLeft  = calcDaysLeft();
            hatchEpoch= calcHatchEpoch();
        }

        // ── Climate phase label ──────────────────────────────────────────────
        String phaseStr = "";
        if (prof == PROFILE_CLIMATE_CHAMBER) {
            if (rs.heaterOn)      phaseStr = "HEAT";
            else if (rs.coolerOn) phaseStr = "COOL";
            else                  phaseStr = "IDLE";
        }

        // ── Build telemetry URL ──────────────────────────────────────────────
        String url = String(GOOGLE_SCRIPT_URL)
            + "?id="       + String(DEVICE_ID)
            + "&fw="       + String(FW_VERSION)
            + "&profile="  + (prof == PROFILE_EGG_INCUBATOR ? "EGG" : "CLIMATE")
            + "&temp="     + urlEncode(tempStr)
            + "&hum="      + urlEncode(humStr)
            + "&setTemp="  + String(tempSP, 1)
            + "&setHum="   + String((int)humSP)
            + "&mode="     + (ctrlMode == MODE_AUTO ? "AUTO" : "MANUAL")
            + "&heater="   + (rs.heaterOn    ? "1" : "0")
            + "&cooler="   + (rs.coolerOn    ? "1" : "0")
            + "&humidifier="+(rs.humidifierOn ? "1" : "0")
            + "&fan="      + (rs.fanOn       ? "1" : "0")
            + "&pump="     + (rs.pumpOn      ? "1" : "0")
            + "&turner="   + (rs.turnerOn    ? "1" : "0");

        if (prof == PROFILE_EGG_INCUBATOR) {
            url += "&day="        + String(day);
            url += "&daysLeft="   + String(daysLeft);
            url += "&hatchEpoch=" + String(hatchEpoch);
        } else {
            url += "&phase=" + urlEncode(phaseStr);
        }

        if (strlen(CLOUD_TOKEN) > 0) {
            url += "&token=" + urlEncode(String(CLOUD_TOKEN));
        }

        // ── Send telemetry ───────────────────────────────────────────────────
        Serial.println("[CLOUD] Sending telemetry...");

        auto sendWithRetries = [&](const String &reqUrl) -> int {
            for (int attempt = 0; attempt < CLOUD_HTTP_MAX_RETRIES; ++attempt) {
                WiFiClientSecure client;
                client.setInsecure();

                HTTPClient http;
                http.setTimeout(10000);
                http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                http.begin(client, reqUrl);

                Serial.printf("[CLOUD] Request: %s (attempt %d)\n", reqUrl.c_str(), attempt+1);
                int code = http.GET();

                String body = "";
                if (code > 0) body = http.getString();

                if (code > 0) {
                    Serial.printf("[CLOUD] Response code: %d\n", code);
                    if (body.length() > 0) Serial.printf("[CLOUD] Body: %s\n", body.c_str());
                    http.end();
                    return code;
                } else {
                    Serial.printf("[CLOUD] HTTP failed: %s\n", http.errorToString(code).c_str());
                    http.end();
                    // backoff
                    uint32_t backoff = CLOUD_HTTP_BACKOFF_MS * (1UL << attempt);
                    vTaskDelay(pdMS_TO_TICKS(backoff));
                }
            }
            return -1;
        };

        int httpCode = sendWithRetries(url);

        if (httpCode > 0) {
            Serial.printf("[CLOUD] HTTP %d\n", httpCode);
            if (httpCode != 200) {
                if (millis() - lastHttpErrorMs > HTTP_ERROR_THROTTLE_MS) {
                    pushError("SERVER_ERROR", ("HTTP " + String(httpCode)).c_str());
                    lastHttpErrorMs = millis();
                }
            }
        } else {
            Serial.printf("[CLOUD] HTTP failed after retries: %d\n", httpCode);
            if (millis() - lastHttpErrorMs > HTTP_ERROR_THROTTLE_MS) {
                pushError("HTTP_FAIL", "Upload failed");
                lastHttpErrorMs = millis();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(CLOUD_TELEMETRY_INTERVAL_MS));
    }
}
