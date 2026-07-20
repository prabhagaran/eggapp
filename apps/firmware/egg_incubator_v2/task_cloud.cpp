#include "task_cloud.h"
#include "globals.h"
#include "config.h"
#include "incubator_logic.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// URL-encode src, appending result into dst (null-terminated, bounded by dstLen)
// ─────────────────────────────────────────────────────────────────────────────
static void urlEncodeAppend(char* dst, size_t dstLen, const char* src) {
    size_t pos = strlen(dst);
    for (; *src; ++src) {
        if (pos + 4 >= dstLen) break;
        char c = *src;
        if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[pos++] = c;
        } else {
            snprintf(dst + pos, dstLen - pos, "%%%02X", (uint8_t)c);
            pos += 3;
        }
    }
    dst[pos] = '\0';
}

// Log a URL, redacting the token value so it never appears in serial output.
static void logUrl(const char* prefix, const char* url) {
    const char* tok = strstr(url, "&token=");
    if (tok) {
        Serial.printf("%s %.*s [token redacted]\n", prefix, (int)(tok - url), url);
    } else {
        Serial.printf("%s %s\n", prefix, url);
    }
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
            logUrl("[CLOUD] Retrying telemetry:", msg.url);
            int code = http.GET();
            if (code > 0) http.getString();  // drain
            http.end();

            // Same success criterion as the first-attempt path: 2xx/3xx only.
            // 404/500 etc. are server failures and must retry, not "succeed".
            if (code >= 200 && code < 400) {
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
            // Reconnect is handled exclusively by task_wifi_manager; just wait.
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

            // Build error URL into a stack buffer — no heap String churn
            char url[768];
            snprintf(url, sizeof(url), "%s?id=%s&fw=%s&profile=%s&error=",
                GOOGLE_SCRIPT_URL, DEVICE_ID, FW_VERSION,
                errProf == PROFILE_EGG_INCUBATOR ? "EGG" : "CLIMATE");
            urlEncodeAppend(url, sizeof(url), incomingErr.type);
            strncat(url, "&msg=", sizeof(url) - strlen(url) - 1);
            urlEncodeAppend(url, sizeof(url), incomingErr.message);
            if (strlen(CLOUD_TOKEN) > 0) {
                strncat(url, "&token=", sizeof(url) - strlen(url) - 1);
                urlEncodeAppend(url, sizeof(url), CLOUD_TOKEN);
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
                http.begin(client, String(url));
                logUrl("[CLOUD] Request:", url);
                int code = http.GET();
                if (code > 0) {
                    http.getString();  // drain body; response code is sufficient
                    Serial.printf("[CLOUD] Response code: %d\n", code);
                } else {
                    Serial.printf("[CLOUD] HTTP failed: %s\n", http.errorToString(code).c_str());
                    TelemetryMsg_t t = {};
                    strncpy(t.url, url, sizeof(t.url) - 1);
                    t.attempts = 1;
                    t.nextAttemptMs = millis() + CLOUD_HTTP_BACKOFF_MS;
                    xQueueSend(telemetryQueue, &t, 0);
                }
                http.end();
            }

            vTaskDelay(pdMS_TO_TICKS(500));  // brief gap between error sends
        }

        // ── Report errors that were dropped while the queue was full ────────
        {
            uint32_t dropped = gErrorsDropped.exchange(0, std::memory_order_acq_rel);
            if (dropped > 0) {
                char msg[40];
                snprintf(msg, sizeof(msg), "%lu errors dropped (queue full)", (unsigned long)dropped);
                pushError("ERR_DROPPED", msg);
                Serial.printf("[CLOUD] %s\n", msg);
            }
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
        char tempStr[8], humStr[8];
        if (temp < -100.0f || temp > 100.0f) strcpy(tempStr, "NA");
        else snprintf(tempStr, sizeof(tempStr), "%.1f", temp);
        if (hum < 0.0f || hum > 100.0f) strcpy(humStr, "NA");
        else snprintf(humStr, sizeof(humStr), "%d", (int)hum);

        // ── Incubation-specific data ─────────────────────────────────────────
        int      day       = 0;
        int      daysLeft  = -1;
        uint32_t hatchEpoch = 0;

        if (prof == PROFILE_EGG_INCUBATOR) {
            day        = calcIncubationDay();
            daysLeft   = calcDaysLeft();
            hatchEpoch = calcHatchEpoch();
        }

        // ── Build telemetry URL into stack buffer — no heap String churn ─────
        char url[768];
        snprintf(url, sizeof(url),
            "%s?id=%s&fw=%s&profile=%s&temp=%s&hum=%s"
            "&setTemp=%.1f&setHum=%d&mode=%s"
            "&heater=%d&cooler=%d&humidifier=%d&fan=%d&pump=%d&turner=%d",
            GOOGLE_SCRIPT_URL, DEVICE_ID, FW_VERSION,
            prof == PROFILE_EGG_INCUBATOR ? "EGG" : "CLIMATE",
            tempStr, humStr,
            tempSP, (int)humSP,
            ctrlMode == MODE_AUTO ? "AUTO" : "MANUAL",
            rs.heaterOn ? 1 : 0, rs.coolerOn ? 1 : 0,
            rs.humidifierOn ? 1 : 0, rs.fanOn ? 1 : 0,
            rs.pumpOn ? 1 : 0, rs.turnerOn ? 1 : 0);

        if (prof == PROFILE_EGG_INCUBATOR) {
            char extra[64];
            snprintf(extra, sizeof(extra), "&day=%d&daysLeft=%d&hatchEpoch=%lu",
                     day, daysLeft, (unsigned long)hatchEpoch);
            strncat(url, extra, sizeof(url) - strlen(url) - 1);
        } else {
            const char* phase = rs.heaterOn ? "HEAT" : rs.coolerOn ? "COOL" : "IDLE";
            strncat(url, "&phase=", sizeof(url) - strlen(url) - 1);
            urlEncodeAppend(url, sizeof(url), phase);
        }

        if (strlen(CLOUD_TOKEN) > 0) {
            strncat(url, "&token=", sizeof(url) - strlen(url) - 1);
            urlEncodeAppend(url, sizeof(url), CLOUD_TOKEN);
        }

        // ── Send telemetry ───────────────────────────────────────────────────
        logUrl("[CLOUD] Sending telemetry:", url);

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
            http.begin(client, String(url));
            int code = http.GET();
            if (code > 0) {
                http.getString();  // drain response body
                Serial.printf("[CLOUD] HTTP %d\n", code);
                if (code != 200 && millis() - lastHttpErrorMs > HTTP_ERROR_THROTTLE_MS) {
                    char errmsg[16];
                    snprintf(errmsg, sizeof(errmsg), "HTTP %d", code);
                    pushError("SERVER_ERROR", errmsg);
                    lastHttpErrorMs = millis();
                }
            } else {
                Serial.printf("[CLOUD] HTTP failed: %s — enqueueing for retry\n",
                              http.errorToString(code).c_str());
                TelemetryMsg_t t = {};
                strncpy(t.url, url, sizeof(t.url) - 1);
                t.attempts = 1;
                t.nextAttemptMs = millis() + CLOUD_HTTP_BACKOFF_MS;
                xQueueSend(telemetryQueue, &t, 0);
                if (millis() - lastHttpErrorMs > HTTP_ERROR_THROTTLE_MS) {
                    pushError("HTTP_FAIL", "Upload queued for retry");
                    lastHttpErrorMs = millis();
                }
            }
            http.end();
        }

        vTaskDelay(pdMS_TO_TICKS(CLOUD_TELEMETRY_INTERVAL_MS));
    }
}
