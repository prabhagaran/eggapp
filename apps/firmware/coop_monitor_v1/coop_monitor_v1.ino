// ─────────────────────────────────────────────────────────────────────────────
// Coop monitoring node (ADR 0009)
//
// Sensor-only device for the bird house. Publishes profile:"COOP"
// telemetry on the shared topic tree (docs/iot/telemetry-contract.md) and
// nothing else — no relays, no control loop, no display, no commands.
//
// Deliberately a plain Arduino loop rather than the FreeRTOS task set the
// incubator uses: there is exactly one periodic job here and no
// concurrent access to shared state, so tasks and mutexes would be
// ceremony without benefit. If actuator control ever lands on a coop node,
// revisit that.
//
// WiFi is provisioned from a phone: on first boot the node raises a
// "COOP_SETUP" access point serving a captive portal, and the chosen
// network is stored in NVS (wifi_manager.cpp). To switch networks later,
// power on then press and hold BOOT within the first few seconds — no
// reflash needed.
//
// Broker credentials live in secrets.h, which is gitignored — copy
// secrets.h.example and fill it in. WiFi credentials are deliberately NOT
// there, so the same image works on any site.
// ─────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "config.h"
#include "secrets.h"
#include "sensors.h"
#include "wifi_manager.h"

static WiFiClient   wifiClient;
static PubSubClient mqttClient(wifiClient);

static char statusTopic[64];
static char telemetryTopic[64];

static unsigned long lastPublishMs   = 0;
static unsigned long lastReconnectMs = 0;
static unsigned long lastSensorMs    = 0;
static CoopReadings_t readings;

// Appends `,"key":value` when valid, `,"key":null` when the sensor is
// fitted but faulted. A channel that isn't compiled in never reaches here
// at all — its absence from the payload is what tells the consumer "no
// sensor", which is a different state from null (telemetry-contract.md).
static void appendField(char* buf, size_t bufSize, const char* key,
                        bool valid, float value, int decimals) {
    char one[32];
    if (valid) snprintf(one, sizeof(one), ",\"%s\":%.*f", key, decimals, value);
    else       snprintf(one, sizeof(one), ",\"%s\":null", key);
    strncat(buf, one, bufSize - strlen(buf) - 1);
}

static void publishTelemetry(void) {
    char payload[MQTT_BUFFER_SIZE];
    snprintf(payload, sizeof(payload),
             "{\"id\":\"%s\",\"fw\":\"%s\",\"profile\":\"COOP\"",
             DEVICE_ID, FW_VERSION);

    // In simulation mode every channel is emitted regardless of the HAS_*
    // flags — the whole point is to exercise the full dashboard with no
    // hardware attached.
#if HAS_TEMP_HUM || SIMULATE_SENSORS
    appendField(payload, sizeof(payload), "temp",  readings.temp_valid,  readings.temp_c,       1);
    appendField(payload, sizeof(payload), "hum",   readings.hum_valid,   readings.humidity_pct, 1);
#endif
#if HAS_CO2_SENSOR || SIMULATE_SENSORS
    appendField(payload, sizeof(payload), "co2",   readings.co2_valid,   readings.co2_ppm,      0);
#endif
#if HAS_NH3_SENSOR || SIMULATE_SENSORS
    appendField(payload, sizeof(payload), "nh3",   readings.nh3_valid,   readings.nh3_ppm,      1);
#endif
#if HAS_LIGHT_SENSOR || SIMULATE_SENSORS
    appendField(payload, sizeof(payload), "lux",   readings.light_valid, readings.light_lux,    0);
#endif
#if HAS_FEED_LEVEL || SIMULATE_SENSORS
    appendField(payload, sizeof(payload), "feed",  readings.feed_valid,  readings.feed_pct,     1);
#endif
#if HAS_WATER_LEVEL || SIMULATE_SENSORS
    appendField(payload, sizeof(payload), "water", readings.water_valid, readings.water_pct,    1);
#endif

    // Emitted only when true, so a real device's payload is unchanged.
    // Its presence is what marks the whole reading as fabricated.
    if (readings.simulated) {
        strncat(payload, ",\"sim\":1", sizeof(payload) - strlen(payload) - 1);
    }

    strncat(payload, "}", sizeof(payload) - strlen(payload) - 1);

    bool sent = mqttClient.publish(telemetryTopic, payload, false /* not retained */);
    Serial.printf("[MQTT] Telemetry %s (%u bytes): %s\n",
                  sent ? "ok" : "FAILED", (unsigned)strlen(payload), payload);
}

void setup(void) {
    Serial.begin(115200);
    delay(100);
    Serial.printf("\n[COOP] %s fw %s starting\n", DEVICE_ID, FW_VERSION);

    snprintf(statusTopic,    sizeof(statusTopic),    "%s/%s/status",    MQTT_TOPIC_PREFIX, DEVICE_ID);
    snprintf(telemetryTopic, sizeof(telemetryTopic), "%s/%s/telemetry", MQTT_TOPIC_PREFIX, DEVICE_ID);

    sensorsBegin();
    // Raises the captive portal if there is no reachable stored network,
    // or if the BOOT button is held. Non-blocking either way — loop()
    // services it, so sampling continues while it's open.
    wifiManagerBegin();

    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient.setKeepAlive(MQTT_KEEPALIVE_SEC);
    mqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);

    // Prime the first reading so the first publish isn't all-null.
    sensorsRead(&readings);
    lastSensorMs = millis();
}

void loop(void) {
    // Sensors are read on their own cadence, independent of publishing —
    // a broker outage must not also stop us sampling.
    if (millis() - lastSensorMs >= SENSOR_INTERVAL_MS) {
        lastSensorMs = millis();
        sensorsRead(&readings);
    }

    // Services the portal when open, reconnects with back-off otherwise.
    // False means no usable link yet — keep sampling, publish nothing.
    if (!wifiManagerLoop()) {
        delay(50);
        return;
    }

    if (!mqttClient.connected()) {
        if (millis() - lastReconnectMs < MQTT_RECONNECT_BACKOFF_MS) return;
        lastReconnectMs = millis();

        Serial.printf("[MQTT] Connecting to %s:%d as %s...\n",
                      MQTT_BROKER_HOST, MQTT_BROKER_PORT, DEVICE_ID);
        // Same LWT contract as the incubator: retained "offline" set as the
        // will, retained "online" published on connect. device-lifecycle.md
        // treats this topic as the only source of the offline transition —
        // silence on telemetry never implies offline.
        bool ok = mqttClient.connect(DEVICE_ID, MQTT_USERNAME, MQTT_PASSWORD,
                                     statusTopic, 1, true, "offline", true);
        if (ok) {
            Serial.println("[MQTT] Connected");
            mqttClient.publish(statusTopic, "online", true /* retained */);
        } else {
            Serial.printf("[MQTT] Connect failed, rc=%d\n", mqttClient.state());
            return;
        }
    }

    mqttClient.loop();   // keepalive

    if (millis() - lastPublishMs >= MQTT_TELEMETRY_INTERVAL_MS || lastPublishMs == 0) {
        lastPublishMs = millis();
        publishTelemetry();
    }

    delay(50);
}
