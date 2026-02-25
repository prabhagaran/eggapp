// ─────────────────────────────────────────────────────────
// ClimateCloud – Integration Example
// Shows how to integrate ClimateCloud MQTT into the
// existing egg incubator ESP32 RTOS firmware
// ─────────────────────────────────────────────────────────
//
// INSTRUCTIONS:
// 1. Copy climatecloud_mqtt.h and climatecloud_mqtt.cpp
//    into your Arduino project folder
// 2. Install PubSubClient and ArduinoJson libraries
// 3. Add the code snippets below to your main .ino file
// 4. Replace credentials with those from ClimateCloud registration
//
// ─────────────────────────────────────────────────────────

// ═══════════════════════════════════════════════
// STEP 1: Add includes at the top of your .ino
// ═══════════════════════════════════════════════

#include "climatecloud_mqtt.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>


// ═══════════════════════════════════════════════
// STEP 2: Create instance and configure credentials
// ═══════════════════════════════════════════════

ClimateCloudMQTT ccMqtt;

// Credentials from ClimateCloud device registration
#define CC_BROKER "broker.emqx.io"
#define CC_PORT 8883
#define CC_DEVICE_UID "DEV_XXXXXXXXXXXX"  // Your registered device UID
#define CC_MQTT_USER "dev_xxxxxxxxxxxx"   // MQTT username (lowercase UID)
#define CC_MQTT_PASS "your_mqtt_password" // MQTT password from registration

// ═══════════════════════════════════════════════
// STEP 3: Define command handler callback
// ═══════════════════════════════════════════════

void handleCloudCommand(const char *commandId, const char *commandType,
                        JsonObject &payload) {
  Serial.printf("[CLOUD CMD] Type: %s\n", commandType);

  bool success = true;
  const char *msg = "OK";

  if (strcmp(commandType, "set_setpoint") == 0) {
    float newSetpoint = payload["setpoint"] | tempSetpoint;
    if (newSetpoint >= 30.0 && newSetpoint <= 45.0) {
      tempSetpoint = newSetpoint;
      // Save to preferences
      prefs.begin("incubator", false);
      prefs.putFloat("setpoint", tempSetpoint);
      prefs.end();
      Serial.printf("[CLOUD] Setpoint changed to: %.1f\n", tempSetpoint);
    } else {
      success = false;
      msg = "Setpoint out of range (30-45)";
    }
  } else if (strcmp(commandType, "set_humidity_setpoint") == 0) {
    float newHum = payload["humid_setpoint"] | humSetpoint;
    if (newHum >= 0 && newHum <= 100) {
      humSetpoint = newHum;
      prefs.begin("incubator", false);
      prefs.putFloat("humSetpoint", humSetpoint);
      prefs.end();
      Serial.printf("[CLOUD] Humidity setpoint: %.1f\n", humSetpoint);
    }
  } else if (strcmp(commandType, "set_mode") == 0) {
    const char *mode = payload["mode"] | "AUTO";
    heaterMode = (strcmp(mode, "MANUAL") == 0) ? MODE_MANUAL : MODE_AUTO;
    Serial.printf("[CLOUD] Mode set to: %s\n", mode);
  } else if (strcmp(commandType, "set_hysteresis") == 0) {
    float newHyst = payload["hysteresis"] | tempHysteresis;
    if (newHyst >= 0.1 && newHyst <= 5.0) {
      tempHysteresis = newHyst;
    }
  } else if (strcmp(commandType, "restart") == 0) {
    ccMqtt.ackCommand(commandId, true, "Restarting...");
    delay(500);
    ESP.restart();
    return;
  } else {
    success = false;
    msg = "Unknown command type";
  }

  // Acknowledge the command
  ccMqtt.ackCommand(commandId, success, msg);
}

// ═══════════════════════════════════════════════
// STEP 4: Define shadow delta handler
// ═══════════════════════════════════════════════

void handleShadowDelta(JsonObject &delta, int version) {
  Serial.printf("[SHADOW] Delta received, version: %d\n", version);

  if (delta.containsKey("setpoint")) {
    tempSetpoint = delta["setpoint"];
    Serial.printf("[SHADOW] Setpoint synced to: %.1f\n", tempSetpoint);
  }

  if (delta.containsKey("humid_setpoint")) {
    humSetpoint = delta["humid_setpoint"];
  }

  if (delta.containsKey("mode")) {
    const char *mode = delta["mode"] | "AUTO";
    heaterMode = (strcmp(mode, "MANUAL") == 0) ? MODE_MANUAL : MODE_AUTO;
  }

  // Report updated state back to cloud
  StaticJsonDocument<256> doc;
  JsonObject state = doc.to<JsonObject>();
  state["setpoint"] = tempSetpoint;
  state["humid_setpoint"] = humSetpoint;
  state["mode"] = (heaterMode == MODE_AUTO) ? "AUTO" : "MANUAL";
  state["fw_version"] = FW_VERSION;
  ccMqtt.reportState(state);
}

// ═══════════════════════════════════════════════
// STEP 5: Add FreeRTOS task for ClimateCloud
// ═══════════════════════════════════════════════

void task_climatecloud(void *pvParameters) {

  // Initialize MQTT
  ccMqtt.begin(CC_BROKER, CC_PORT, CC_DEVICE_UID, CC_MQTT_USER, CC_MQTT_PASS);
  ccMqtt.onCommand(handleCloudCommand);
  ccMqtt.onShadowDelta(handleShadowDelta);

  unsigned long lastTelemetrySend = 0;

  for (;;) {

    // Run MQTT loop (handles reconnect + incoming messages)
    ccMqtt.loop();

    // Send telemetry every 30 seconds
    if (millis() - lastTelemetrySend > 30000 && ccMqtt.isConnected()) {

      float t = 0.0, h = 0.0;

      if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(200))) {
        t = gSensorData.temp_ds18b20;
        h = gSensorData.humidity_dht;
        xSemaphoreGive(sensorMutex);
      }

      ccMqtt.sendTelemetry(t,            // temperature
                           h,            // humidity
                           heaterOn,     // heater status
                           coolerOn,     // cooler status
                           humidifierOn, // humidifier status
                           tempSetpoint, // temperature setpoint
                           humSetpoint,  // humidity setpoint
                           (heaterMode == MODE_AUTO) ? "AUTO"
                                                     : "MANUAL", // mode
                           FW_VERSION // firmware version
      );

      lastTelemetrySend = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ═══════════════════════════════════════════════
// STEP 6: Create the task in setup()
// ═══════════════════════════════════════════════
//
// Add this inside your setup() function,
// alongside your existing xTaskCreatePinnedToCore calls:
//
//   xTaskCreatePinnedToCore(
//     task_climatecloud,    // Task function
//     "ClimateCloud",       // Name
//     8192,                 // Stack size (larger for TLS)
//     NULL,                 // Parameters
//     1,                    // Priority
//     NULL,                 // Task handle
//     0                     // Core 0 (leave Core 1 for control)
//   );
//
// ═══════════════════════════════════════════════
