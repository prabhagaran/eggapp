// ─────────────────────────────────────────────────────────
// ClimateCloud – ESP32 MQTT Client Module
// Drop-in integration with existing egg incubator firmware
// ─────────────────────────────────────────────────────────

#ifndef CLIMATECLOUD_MQTT_H
#define CLIMATECLOUD_MQTT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>


// ══════════════════════════════════════════════
//  CONFIGURATION — Update these for your device
// ══════════════════════════════════════════════

#define CC_MQTT_BROKER "broker.emqx.io"
#define CC_MQTT_PORT 8883                  // TLS port
#define CC_MQTT_USERNAME "your_device_uid" // From ClimateCloud registration
#define CC_MQTT_PASSWORD "your_mqtt_pass"  // From ClimateCloud registration
#define CC_DEVICE_UID "your_device_uid"    // Device UID from registration

// Topic prefix
#define CC_TOPIC_PREFIX "climatecloud/device/"

// Intervals (ms)
#define CC_TELEMETRY_INTERVAL 30000   // Send telemetry every 30s
#define CC_SHADOW_SYNC_INTERVAL 60000 // Request shadow every 60s
#define CC_RECONNECT_INTERVAL 5000    // Reconnect attempt interval

// ══════════════════════════════════════════════
//  CALLBACK TYPE DEFINITIONS
// ══════════════════════════════════════════════

// Called when a command is received from the cloud
typedef void (*CommandCallback)(const char *commandId, const char *commandType,
                                JsonObject &payload);

// Called when shadow delta is received (desired != reported)
typedef void (*ShadowDeltaCallback)(JsonObject &delta, int version);

// ══════════════════════════════════════════════
//  CLIMATECLOUD MQTT CLIENT CLASS
// ══════════════════════════════════════════════

class ClimateCloudMQTT {
public:
  ClimateCloudMQTT();

  // Initialize with device credentials
  void begin(const char *broker, int port, const char *deviceUid,
             const char *mqttUsername, const char *mqttPassword);

  // Must be called in loop or FreeRTOS task
  void loop();

  // Set callback handlers
  void onCommand(CommandCallback cb) { _commandCb = cb; }
  void onShadowDelta(ShadowDeltaCallback cb) { _shadowDeltaCb = cb; }

  // ——————— Telemetry ———————
  // Publish sensor data to cloud
  void sendTelemetry(float temperature, float humidity, bool heaterOn,
                     bool coolerOn, bool humidifierOn, float setpoint,
                     float humidSetpoint, const char *mode,
                     const char *fwVersion);

  // ——————— Shadow ———————
  // Report current device state to cloud
  void reportState(JsonObject &state);

  // Request current shadow from cloud
  void requestShadow();

  // ——————— Command ACK ———————
  // Acknowledge a command
  void ackCommand(const char *commandId, bool success,
                  const char *message = nullptr);

  // ——————— Status ———————
  bool isConnected() { return _mqttClient.connected(); }

private:
  WiFiClientSecure _wifiClient;
  PubSubClient _mqttClient;

  const char *_broker;
  int _port;
  const char *_deviceUid;
  const char *_mqttUsername;
  const char *_mqttPassword;

  CommandCallback _commandCb;
  ShadowDeltaCallback _shadowDeltaCb;

  unsigned long _lastTelemetry;
  unsigned long _lastShadowSync;
  unsigned long _lastReconnectAttempt;

  // Topic buffers
  char _topicTelemetry[128];
  char _topicStatus[128];
  char _topicCommand[128];
  char _topicCommandAck[128];
  char _topicShadowUpdate[128];
  char _topicShadowDelta[128];
  char _topicShadowGet[128];

  void _buildTopics();
  void _connect();
  void _onMessage(char *topic, byte *payload, unsigned int length);

  static ClimateCloudMQTT *_instance;
  static void _staticCallback(char *topic, byte *payload, unsigned int length);
};

#endif // CLIMATECLOUD_MQTT_H
