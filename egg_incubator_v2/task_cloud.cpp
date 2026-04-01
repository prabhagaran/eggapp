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

        // ── Process queued telemetry messages (retry/backoff) ───────────────
        // M-7 fix: peek at the queue head before dequeuing. If the earliest
        // item is not yet due, break immediately — no point iterating; all
        // items currently in the queue will be due no sooner than the head.
        // This prevents the tight re-enqueue loop that wasted CPU when the
        // queue was dominated by not-yet-due backoff items.
        size_t queued = uxQueueMessagesWaiting(telemetryQueue);
        for (size_t i = 0; i < queued; ++i) {
            // Non-destructive peek: if head not due yet, stop for this cycle.
            TelemetryMsg_t peek;
            if (xQueuePeek(telemetryQueue, &peek, 0) != pdTRUE) break;
            if ((int32_t)(peek.nextAttemptMs - millis()) > 0) break;

            // Head is due — dequeue and attempt
            TelemetryMsg_t msg;
            if (xQueueReceive(telemetryQueue, &msg, 0) != pdTRUE) break;

            // Try send once
                    WiFiClientSecure client;
                    if (strlen(CLOUD_ROOT_CA) > 10) {
                        client.setCACert(CLOUD_ROOT_CA);
                    } else {
                        client.setInsecure();
                    }
            HTTPClient http;
            http.setTimeout(5000);
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.begin(client, String(msg.url));
            Serial.printf("[CLOUD] Retrying telemetry: %s (attempt %d)\n", msg.url, msg.attempts+1);
            int code = http.GET();
            String body = "";
            if (code > 0) body = http.getString();
            http.end();

            if (code > 0) {
                Serial.printf("[CLOUD] Retry success code=%d\n", code);
            } else {
                // failed: schedule next attempt or drop
                msg.attempts++;
                if (msg.attempts >= CLOUD_HTTP_MAX_RETRIES) {
                    Serial.printf("[CLOUD] Dropping telemetry after %d attempts\n", msg.attempts);
                    pushError("HTTP_FAIL", "Telemetry dropped after retries");
                } else {
                    uint32_t backoff = CLOUD_HTTP_BACKOFF_MS * (1UL << (msg.attempts - 1));
                    msg.nextAttemptMs = millis() + backoff;
                    xQueueSend(telemetryQueue, &msg, 0);
                }
            }
        }

        // ── WiFi check ───────────────────────────────────────────────────────
        // Only proceed if user has enabled Wi-Fi; never auto-reconnect otherwise.
        if (!wifiUserEnabled.load(std::memory_order_acquire)) {
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

            // Read active profile for error context
            ProfileType errProf = PROFILE_EGG_INCUBATOR;
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                errProf = gSettings.activeProfile;
                xSemaphoreGive(settingsMutex);
            }

            // Build URL for this error
            String url = String(GOOGLE_SCRIPT_URL)
                + "?id="      + String(DEVICE_ID)
                + "&fw="      + String(FW_VERSION)
                + "&profile=" + (errProf == PROFILE_EGG_INCUBATOR ? "EGG" : "CLIMATE")
                + "&error="   + urlEncode(String(incomingErr.type))
                + "&msg="     + urlEncode(String(incomingErr.message));

            if (strlen(CLOUD_TOKEN) > 0) {
                url += "&token=" + urlEncode(String(CLOUD_TOKEN));
            }

            Serial.printf("[CLOUD] Error log: %s — %s\n", incomingErr.type, incomingErr.message);

            // Attempt send once; on failure enqueue for retry
            {
                WiFiClientSecure client;
                if (strlen(CLOUD_ROOT_CA) > 10) {
                    client.setCACert(CLOUD_ROOT_CA);
                } else {
                    client.setInsecure();
                }
                HTTPClient http;
                http.setTimeout(5000);
                http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                http.begin(client, url);
                Serial.printf("[CLOUD] Request: %s (attempt 1)\n", url.c_str());
                int code = http.GET();
                String body = "";
                if (code > 0) body = http.getString();
                http.end();

                if (code > 0) {
                    Serial.printf("[CLOUD] Response code: %d\n", code);
                    if (body.length() > 0) Serial.printf("[CLOUD] Body: %s\n", body.c_str());
                } else {
                    Serial.printf("[CLOUD] HTTP failed: %s\n", http.errorToString(code).c_str());
                    // Enqueue for retry
                    TelemetryMsg_t t = {};
                    strncpy(t.url, url.c_str(), sizeof(t.url) - 1);
                    t.attempts = 1;
                    t.nextAttemptMs = millis() + CLOUD_HTTP_BACKOFF_MS;
                    xQueueSend(telemetryQueue, &t, 0);
                }
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

        // Single-attempt send; on failure enqueue for retry
        {
            WiFiClientSecure client;
            if (strlen(CLOUD_ROOT_CA) > 10) {
                client.setCACert(CLOUD_ROOT_CA);
            } else {
                client.setInsecure();
            }
            HTTPClient http;
            http.setTimeout(5000);
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.begin(client, url);
            Serial.printf("[CLOUD] Request: %s (attempt 1)\n", url.c_str());
            int code = http.GET();
            String body = "";
            if (code > 0) body = http.getString();
            http.end();

            if (code > 0) {
                Serial.printf("[CLOUD] HTTP %d\n", code);
                if (code != 200) {
                    if (millis() - lastHttpErrorMs > HTTP_ERROR_THROTTLE_MS) {
                        pushError("SERVER_ERROR", ("HTTP " + String(code)).c_str());
                        lastHttpErrorMs = millis();
                    }
                }
            } else {
                Serial.printf("[CLOUD] HTTP failed: %s — enqueueing for retry\n", http.errorToString(code).c_str());
                TelemetryMsg_t t = {};
                strncpy(t.url, url.c_str(), sizeof(t.url) - 1);
                t.attempts = 1;
                t.nextAttemptMs = millis() + CLOUD_HTTP_BACKOFF_MS;
                xQueueSend(telemetryQueue, &t, 0);
                if (millis() - lastHttpErrorMs > HTTP_ERROR_THROTTLE_MS) {
                    pushError("HTTP_FAIL", "Upload queued for retry");
                    lastHttpErrorMs = millis();
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(CLOUD_TELEMETRY_INTERVAL_MS));
    }
}
