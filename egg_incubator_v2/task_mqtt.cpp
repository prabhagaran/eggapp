#include "task_mqtt.h"
#include "globals.h"
#include "config.h"
#include "incubator_logic.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <stdlib.h>

// ─────────────────────────────────────────────────────────────────────────────
// COMMAND HANDLING (US-INC-003 — setpoint config with ack)
//
// Hand-parsed (no ArduinoJson dependency, matching this file's existing
// hand-built outgoing JSON) — the incoming payload is a small, flat,
// known set of numeric fields, so a strstr+strtof/strtol scan is enough
// and keeps the same no-heap-churn style as the rest of the firmware.
// ─────────────────────────────────────────────────────────────────────────────
// File-scope (not function-local) so the PubSubClient callback below —
// which PubSubClient requires as a free function, with no capture
// context — can reach the client to publish acks and the topic string
// to recognize its own command topic.
static WiFiClient   wifiClient;
static PubSubClient mqttClient(wifiClient);
static char         cmdTopic[64];
static char         cmdAckTopic[64];

static bool extractFloatField(const char* json, const char* key, float* out) {
    char pattern[24];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* p = strstr(json, pattern);
    if (!p) return false;
    *out = strtof(p + strlen(pattern), nullptr);
    return true;
}

static bool extractLongField(const char* json, const char* key, long* out) {
    char pattern[24];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* p = strstr(json, pattern);
    if (!p) return false;
    *out = strtol(p + strlen(pattern), nullptr, 10);
    return true;
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Applies whichever setpoint fields are present in `payload`, clamped to
// the same config.h edit limits the physical button UI already enforces
// (task_ui.cpp). Publishes "received" before taking the mutex and
// "applied" right after — both typically land within the same MQTT
// keepalive tick, but modeled distinctly per the DeviceConfig ack schema.
static void handleCommand(PubSubClient& mqttClient, const char* ackTopic, const char* payload) {
    long version = 0;
    if (!extractLongField(payload, "version", &version)) {
        Serial.println("[MQTT] cmd payload missing version — ignored");
        return;
    }

    char receivedAck[48];
    snprintf(receivedAck, sizeof(receivedAck), "{\"version\":%ld,\"state\":\"received\"}", version);
    mqttClient.publish(ackTopic, receivedAck);

    float tempSetpoint, tempHysteresis, humSetpoint, humHysteresis;
    bool hasTempSP = extractFloatField(payload, "tempSetpoint", &tempSetpoint);
    bool hasTempHyst = extractFloatField(payload, "tempHysteresis", &tempHysteresis);
    bool hasHumSP = extractFloatField(payload, "humSetpoint", &humSetpoint);
    bool hasHumHyst = extractFloatField(payload, "humHysteresis", &humHysteresis);

    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        if (hasTempSP) gSettings.tempSetpoint = clampf(tempSetpoint, TEMP_SETPOINT_MIN, TEMP_SETPOINT_MAX);
        if (hasTempHyst) gSettings.tempHysteresis = clampf(tempHysteresis, TEMP_HYST_MIN, TEMP_HYST_MAX);
        if (hasHumSP) gSettings.humSetpoint = clampf(humSetpoint, HUM_SETPOINT_MIN, HUM_SETPOINT_MAX);
        if (hasHumHyst) gSettings.humHysteresis = clampf(humHysteresis, HUM_HYST_MIN, HUM_HYST_MAX);
        xSemaphoreGive(settingsMutex);
    } else {
        Serial.println("[MQTT] settingsMutex timeout — cmd not applied");
        return;
    }

    char appliedAck[48];
    snprintf(appliedAck, sizeof(appliedAck), "{\"version\":%ld,\"state\":\"applied\"}", version);
    mqttClient.publish(ackTopic, appliedAck);
    Serial.printf("[MQTT] cmd version %ld applied\n", version);
}

// PubSubClient's callback signature has no context parameter, hence the
// file-scope statics above. `payload` is not null-terminated and not
// guaranteed writable, so it's copied into a local stack buffer first.
static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, cmdTopic) != 0) return; // only subscribed topic is cmd, but be explicit

    char buf[MQTT_BUFFER_SIZE];
    unsigned int n = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
    memcpy(buf, payload, n);
    buf[n] = '\0';

    handleCommand(mqttClient, cmdAckTopic, buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// TASK: MQTT TELEMETRY (runs on Core 0 at low priority)
//
// Additive to task_cloud's Google Sheets upload — publishes the same
// snapshot of sensor/setpoint/relay/incubation state as JSON to a local
// MQTT broker. Fully independent: does not read task_cloud's state, does
// not share its queue, and a failure here never affects Sheets uploads
// (or vice versa).
//
// Topics (prefix configurable via MQTT_TOPIC_PREFIX, defaults to
// "eggapp/devices"):
//   <prefix>/<DEVICE_ID>/telemetry — JSON, QoS 0, not retained, published
//                                    every MQTT_TELEMETRY_INTERVAL_MS
//   <prefix>/<DEVICE_ID>/status    — "online" (retained) published right
//                                    after connect; "offline" (retained)
//                                    set as the connection's Last Will,
//                                    so the broker publishes it for us if
//                                    the device drops off ungracefully.
//
// Disabled safely if MQTT_BROKER_HOST is empty (no secrets.h entry yet) —
// same opt-in pattern as the Google Sheets cloud path.
// ─────────────────────────────────────────────────────────────────────────────
void task_mqtt(void* pvParameters) {

    char statusTopic[64];
    char telemetryTopic[64];
    snprintf(statusTopic,    sizeof(statusTopic),    "%s/%s/status",    MQTT_TOPIC_PREFIX, DEVICE_ID);
    snprintf(telemetryTopic, sizeof(telemetryTopic), "%s/%s/telemetry", MQTT_TOPIC_PREFIX, DEVICE_ID);
    snprintf(cmdTopic,       sizeof(cmdTopic),       "%s/%s/cmd",       MQTT_TOPIC_PREFIX, DEVICE_ID);
    snprintf(cmdAckTopic,    sizeof(cmdAckTopic),    "%s/%s/cmd/ack",   MQTT_TOPIC_PREFIX, DEVICE_ID);

    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient.setKeepAlive(MQTT_KEEPALIVE_SEC);
    mqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    mqttClient.setCallback(onMqttMessage);

    unsigned long lastPublishMs   = 0;
    unsigned long lastReconnectMs = 0;

    for (;;) {

        // Feature is opt-in: no broker host configured means "not set up
        // yet" — idle quietly rather than spamming reconnect attempts.
        if (strlen(MQTT_BROKER_HOST) == 0) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // Only proceed if the user has WiFi enabled and it's connected.
        // Reconnect is task_wifi_manager's job; just wait otherwise.
        if (!wifiUserEnabled.load(std::memory_order_acquire) ||
            WiFi.status() != WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // ── (Re)connect ──────────────────────────────────────────────────────
        if (!mqttClient.connected()) {
            if (millis() - lastReconnectMs < MQTT_RECONNECT_BACKOFF_MS) {
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }
            lastReconnectMs = millis();

            Serial.printf("[MQTT] Connecting to %s:%d as %s...\n",
                          MQTT_BROKER_HOST, MQTT_BROKER_PORT, DEVICE_ID);

            bool ok = mqttClient.connect(
                DEVICE_ID,                 // client id
                MQTT_USERNAME, MQTT_PASSWORD,
                statusTopic, 1, true, "offline",  // LWT: retained, QoS1
                true);                      // clean session

            if (ok) {
                Serial.println("[MQTT] Connected");
                mqttClient.publish(statusTopic, "online", true /* retained */);
                mqttClient.subscribe(cmdTopic, 1 /* QoS 1 */);
            } else {
                Serial.printf("[MQTT] Connect failed, rc=%d\n", mqttClient.state());
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }
        }

        // Service the MQTT connection (keepalive pings, incoming packets).
        mqttClient.loop();

        // ── Publish telemetry on schedule ────────────────────────────────────
        if (millis() - lastPublishMs >= MQTT_TELEMETRY_INTERVAL_MS) {
            lastPublishMs = millis();

            // Snapshot all state — same mutex pattern as task_cloud.
            float temp = 0.0f, hum = 0.0f;
            if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                temp = gSensorData.temp_ds18b20;
                hum  = gSensorData.humidity_dht;
                xSemaphoreGive(sensorMutex);
            }

            float           tempSP = DEFAULT_TEMP_SETPOINT, humSP = DEFAULT_HUM_SETPOINT;
            float           tempHyst = DEFAULT_TEMP_HYSTERESIS, humHyst = DEFAULT_HUM_HYSTERESIS;
            ControlMode     ctrlMode = MODE_AUTO;
            ProfileType     prof     = PROFILE_EGG_INCUBATOR;

            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                tempSP   = gSettings.tempSetpoint;
                humSP    = gSettings.humSetpoint;
                tempHyst = gSettings.tempHysteresis;
                humHyst  = gSettings.humHysteresis;
                ctrlMode = gSettings.controlMode;
                prof     = gSettings.activeProfile;
                xSemaphoreGive(settingsMutex);
            }

            RelayState_t rs = {};
            if (xSemaphoreTake(controlMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                rs = gRelayState;
                xSemaphoreGive(controlMutex);
            }

            char tempStr[8], humStr[8];
            if (temp < -100.0f || temp > 100.0f) strcpy(tempStr, "null");
            else snprintf(tempStr, sizeof(tempStr), "%.1f", temp);
            if (hum < 0.0f || hum > 100.0f) strcpy(humStr, "null");
            else snprintf(humStr, sizeof(humStr), "%d", (int)hum);

            int      day       = 0;
            int      daysLeft  = -1;
            uint32_t hatchEpoch = 0;
            if (prof == PROFILE_EGG_INCUBATOR) {
                day        = calcIncubationDay();
                daysLeft   = calcDaysLeft();
                hatchEpoch = calcHatchEpoch();
            }

            // Build JSON manually (stack buffer, no heap/String churn) —
            // same style as task_cloud's URL construction.
            char payload[MQTT_BUFFER_SIZE];
            snprintf(payload, sizeof(payload),
                "{\"id\":\"%s\",\"fw\":\"%s\",\"profile\":\"%s\","
                "\"temp\":%s,\"hum\":%s,\"setTemp\":%.1f,\"setHum\":%d,"
                "\"setTempHyst\":%.1f,\"setHumHyst\":%.1f,"
                "\"mode\":\"%s\",\"heater\":%d,\"cooler\":%d,\"humidifier\":%d,"
                "\"fan\":%d,\"pump\":%d,\"turner\":%d",
                DEVICE_ID, FW_VERSION,
                prof == PROFILE_EGG_INCUBATOR ? "EGG" : "CLIMATE",
                tempStr, humStr, tempSP, (int)humSP,
                tempHyst, humHyst,
                ctrlMode == MODE_AUTO ? "AUTO" : "MANUAL",
                rs.heaterOn ? 1 : 0, rs.coolerOn ? 1 : 0, rs.humidifierOn ? 1 : 0,
                rs.fanOn ? 1 : 0, rs.pumpOn ? 1 : 0, rs.turnerOn ? 1 : 0);

            if (prof == PROFILE_EGG_INCUBATOR) {
                char extra[64];
                snprintf(extra, sizeof(extra), ",\"day\":%d,\"daysLeft\":%d,\"hatchEpoch\":%lu",
                         day, daysLeft, (unsigned long)hatchEpoch);
                strncat(payload, extra, sizeof(payload) - strlen(payload) - 1);
            }
            strncat(payload, "}", sizeof(payload) - strlen(payload) - 1);

            bool sent = mqttClient.publish(telemetryTopic, payload, false /* not retained */);
            Serial.printf("[MQTT] Telemetry publish %s (%u bytes)\n",
                          sent ? "ok" : "FAILED", (unsigned)strlen(payload));
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // keep loop() serviced for keepalive
    }
}
