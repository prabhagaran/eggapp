// ─────────────────────────────────────────────────────────
// ClimateCloud – ESP32 MQTT Client Implementation
// ─────────────────────────────────────────────────────────

#include "climatecloud_mqtt.h"

ClimateCloudMQTT *ClimateCloudMQTT::_instance = nullptr;

ClimateCloudMQTT::ClimateCloudMQTT()
    : _mqttClient(_wifiClient), _broker(nullptr), _port(8883),
      _deviceUid(nullptr), _mqttUsername(nullptr), _mqttPassword(nullptr),
      _commandCb(nullptr), _shadowDeltaCb(nullptr), _lastTelemetry(0),
      _lastShadowSync(0), _lastReconnectAttempt(0) {
  _instance = this;
}

void ClimateCloudMQTT::begin(const char *broker, int port,
                             const char *deviceUid, const char *mqttUsername,
                             const char *mqttPassword) {
  _broker = broker;
  _port = port;
  _deviceUid = deviceUid;
  _mqttUsername = mqttUsername;
  _mqttPassword = mqttPassword;

  _buildTopics();

  _wifiClient.setInsecure(); // For testing; use CA cert in production
  _mqttClient.setServer(_broker, _port);
  _mqttClient.setCallback(_staticCallback);
  _mqttClient.setBufferSize(1024);
  _mqttClient.setKeepAlive(60);

  Serial.println("[CC-MQTT] Initialized");
}

void ClimateCloudMQTT::_buildTopics() {
  snprintf(_topicTelemetry, sizeof(_topicTelemetry), "%s%s/telemetry",
           CC_TOPIC_PREFIX, _deviceUid);
  snprintf(_topicStatus, sizeof(_topicStatus), "%s%s/status", CC_TOPIC_PREFIX,
           _deviceUid);
  snprintf(_topicCommand, sizeof(_topicCommand), "%s%s/command",
           CC_TOPIC_PREFIX, _deviceUid);
  snprintf(_topicCommandAck, sizeof(_topicCommandAck), "%s%s/command/ack",
           CC_TOPIC_PREFIX, _deviceUid);
  snprintf(_topicShadowUpdate, sizeof(_topicShadowUpdate), "%s%s/shadow/update",
           CC_TOPIC_PREFIX, _deviceUid);
  snprintf(_topicShadowDelta, sizeof(_topicShadowDelta), "%s%s/shadow/delta",
           CC_TOPIC_PREFIX, _deviceUid);
  snprintf(_topicShadowGet, sizeof(_topicShadowGet), "%s%s/shadow/get",
           CC_TOPIC_PREFIX, _deviceUid);
}

void ClimateCloudMQTT::_connect() {
  if (_mqttClient.connected())
    return;
  if (WiFi.status() != WL_CONNECTED)
    return;

  unsigned long now = millis();
  if (now - _lastReconnectAttempt < CC_RECONNECT_INTERVAL)
    return;
  _lastReconnectAttempt = now;

  Serial.printf("[CC-MQTT] Connecting to %s:%d...\n", _broker, _port);

  // Build client ID
  char clientId[64];
  snprintf(clientId, sizeof(clientId), "cc-%s-%lu", _deviceUid,
           millis() % 10000);

  // Set Last Will (offline status)
  char willPayload[] = "{\"status\":\"offline\"}";

  if (_mqttClient.connect(clientId, _mqttUsername, _mqttPassword, _topicStatus,
                          1, true, willPayload)) {
    Serial.println("[CC-MQTT] Connected!");

    // Publish online status
    _mqttClient.publish(_topicStatus, "{\"status\":\"online\"}", true);

    // Subscribe to incoming topics
    _mqttClient.subscribe(_topicCommand, 1);
    _mqttClient.subscribe(_topicShadowDelta, 1);
    _mqttClient.subscribe(_topicShadowGet, 1);

    Serial.println("[CC-MQTT] Subscribed to command & shadow topics");

    // Request current shadow on connect
    requestShadow();
  } else {
    Serial.printf("[CC-MQTT] Connection failed, rc=%d\n", _mqttClient.state());
  }
}

void ClimateCloudMQTT::loop() {
  if (!_mqttClient.connected()) {
    _connect();
  }
  _mqttClient.loop();
}

// ══════════════════════════════════════════════
//  TELEMETRY
// ══════════════════════════════════════════════

void ClimateCloudMQTT::sendTelemetry(float temperature, float humidity,
                                     bool heaterOn, bool coolerOn,
                                     bool humidifierOn, float setpoint,
                                     float humidSetpoint, const char *mode,
                                     const char *fwVersion) {
  if (!_mqttClient.connected())
    return;

  StaticJsonDocument<512> doc;
  doc["device_id"] = _deviceUid;
  doc["temperature"] = serialized(String(temperature, 1));
  doc["humidity"] = serialized(String(humidity, 0));
  doc["heater"] = heaterOn;
  doc["cooler"] = coolerOn;
  doc["humidifier"] = humidifierOn;
  doc["setpoint"] = serialized(String(setpoint, 1));
  doc["humid_setpoint"] = serialized(String(humidSetpoint, 1));
  doc["mode"] = mode;
  doc["fw_version"] = fwVersion;
  doc["uptime"] = millis() / 1000;

  char buffer[512];
  size_t len = serializeJson(doc, buffer);

  if (_mqttClient.publish(_topicTelemetry, buffer, len)) {
    Serial.printf("[CC-MQTT] Telemetry sent: T=%.1f H=%.0f\n", temperature,
                  humidity);
  } else {
    Serial.println("[CC-MQTT] Telemetry publish failed");
  }
}

// ══════════════════════════════════════════════
//  SHADOW
// ══════════════════════════════════════════════

void ClimateCloudMQTT::reportState(JsonObject &state) {
  if (!_mqttClient.connected())
    return;

  StaticJsonDocument<512> doc;
  doc["reported"] = state;

  char buffer[512];
  size_t len = serializeJson(doc, buffer);
  _mqttClient.publish(_topicShadowUpdate, buffer, len);
}

void ClimateCloudMQTT::requestShadow() {
  if (!_mqttClient.connected())
    return;

  StaticJsonDocument<64> doc;
  doc["request"] = "get";

  char buffer[64];
  size_t len = serializeJson(doc, buffer);
  _mqttClient.publish(_topicShadowGet, buffer, len);
}

// ══════════════════════════════════════════════
//  COMMAND ACK
// ══════════════════════════════════════════════

void ClimateCloudMQTT::ackCommand(const char *commandId, bool success,
                                  const char *message) {
  if (!_mqttClient.connected())
    return;

  StaticJsonDocument<256> doc;
  doc["command_id"] = commandId;
  doc["status"] = success ? "applied" : "error";
  if (message)
    doc["message"] = message;

  char buffer[256];
  size_t len = serializeJson(doc, buffer);
  _mqttClient.publish(_topicCommandAck, buffer, len);

  Serial.printf("[CC-MQTT] Command %s %s\n", commandId,
                success ? "ACK" : "NACK");
}

// ══════════════════════════════════════════════
//  MESSAGE HANDLER
// ══════════════════════════════════════════════

void ClimateCloudMQTT::_staticCallback(char *topic, byte *payload,
                                       unsigned int length) {
  if (_instance)
    _instance->_onMessage(topic, payload, length);
}

void ClimateCloudMQTT::_onMessage(char *topic, byte *payload,
                                  unsigned int length) {
  // Safety: null-terminate
  char message[1024];
  size_t copyLen = min((unsigned int)1023, length);
  memcpy(message, payload, copyLen);
  message[copyLen] = '\0';

  Serial.printf("[CC-MQTT] Received on %s: %s\n", topic, message);

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, message);
  if (err) {
    Serial.printf("[CC-MQTT] JSON parse error: %s\n", err.c_str());
    return;
  }

  // ── Command Handler ──
  if (strcmp(topic, _topicCommand) == 0) {
    if (_commandCb) {
      const char *cmdId = doc["command_id"] | "unknown";
      const char *cmdType = doc["command_type"] | "unknown";
      JsonObject payload = doc.as<JsonObject>();
      _commandCb(cmdId, cmdType, payload);
    }
  }

  // ── Shadow Delta Handler ──
  else if (strcmp(topic, _topicShadowDelta) == 0) {
    if (_shadowDeltaCb) {
      JsonObject delta = doc["delta"].as<JsonObject>();
      int version = doc["version"] | 0;
      _shadowDeltaCb(delta, version);
    }
  }
}
