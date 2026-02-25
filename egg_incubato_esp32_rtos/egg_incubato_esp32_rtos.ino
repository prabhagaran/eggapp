#include <Arduino.h>
#include <Wire.h>
#include <ezButton.h>
#include "RTClib.h"
#include "oled_ui.h"
#include <DHT.h>

#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>



/* ================= PINS ================= */
#define BTN_UP 32
#define BTN_DOWN 33
#define BTN_OK 25

#define I2C_SDA 21
#define I2C_SCL 22

#define DHT_PIN 4
#define DHT_TYPE DHT11

#define DS18B20_PIN 18

/* ================= RELAY PINS ================= */

#define RELAY_HEATER 26
#define RELAY_COOLER 27
#define RELAY_HUMIDIFIER 14

#define RELAY_ON LOW
#define RELAY_OFF HIGH

#define DEVICE_ID "INCUBATOR_01"
#define FW_VERSION "1.0.0"



OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);


DHT dht(DHT_PIN, DHT_TYPE);

/* ================= UI EVENTS ================= */
enum UiEvent {
  UI_EVT_UP,
  UI_EVT_DOWN,
  UI_EVT_OK
};

/* ================= MENU ================= */
enum MenuItem {
  MENU_CONTROLLER_MODE = 0,
  MENU_SET_ENVIRONMENT,
  MENU_SETTINGS,
  MENU_EXIT,
  MENU_COUNT
};

enum ControllerModeItem {
  MODE_EGG_INCUBATOR = 0,
  MODE_CLIMATE_CHAMBER,
  MODE_THERMOSTAT,
  MODE_COMMON,
  MODE_COUNT
};

enum EnvironmentItem {
  ENV_TEMPERATURE = 0,
  ENV_HYSTERESIS,

  ENV_HUMIDITY,
  ENV_INCUBATION_DAY,
  ENV_TURNING,
  ENV_BACK,
  ENV_COUNT
};

enum SettingsItem {
  SET_TIME_DATE = 0,
  SET_WIFI,
  SET_MODE,
  SET_DEVICE_INFO,
  SET_FACTORY_RESET,
  SET_BACK,
  SET_COUNT
};

/* ================= UI STATES ================= */
enum UiState {
  UI_HOME,
  UI_MAIN_MENU,
  UI_CONTROLLER_MODE_MENU,
  UI_SET_ENV_MENU,
  UI_SETTINGS_MENU,
  UI_ENV_TEMPERATURE,
  UI_ENV_HYSTERESIS,
  UI_ENV_HYSTERESIS_MENU,
  UI_ENV_HYST_TEMP_EDIT,
  UI_ENV_HYST_HUM_EDIT,
  UI_ENV_HUMIDITY,
  UI_ENV_DAY,
  UI_ENV_TURNING,
  UI_MODE_MENU,
  UI_MANUAL_CONTROL_MENU
};
UiState uiState = UI_HOME;
// Heater control mode

enum ControlMode {
  MODE_AUTO = 0,
  MODE_MANUAL
};
ControlMode heaterMode = MODE_AUTO;


/* ================= SENSOR DATA ================= */
typedef struct {
  float temp_ds18b20;  // Main temperature
  float humidity_dht;  // Humidity only
} SensorData_t;
SensorData_t gSensorData;
SemaphoreHandle_t sensorMutex;

/* ================= RTC DATA ================= */
typedef struct {
  DateTime now;
  uint32_t epoch;
} RtcTime_t;
RtcTime_t gRtcTime;
SemaphoreHandle_t rtcMutex;

typedef struct {
  char type[20];
  char message[60];
} ErrorMsg_t;
QueueHandle_t errorQueue;


/* ================= GLOBALS ================= */
QueueHandle_t uiEventQueue;
RTC_DS1307 rtc;

ezButton btnUp(BTN_UP);
ezButton btnDown(BTN_DOWN);
ezButton btnOk(BTN_OK);

int mainMenuIndex = 0;
int controllerModeIndex = 0;
int environmentMenuIndex = 0;
int settingsMenuIndex = 0;
int lastMenuIndex = -1;
int lastMinute = -1;
int modeMenuIndex = 0;
int manualControlIndex = 0;
int hysteresisMenuIndex = 0;
volatile bool overTempFault = false;


unsigned long lastSensorUiUpdate = 0;
Preferences prefs;
unsigned long lastTempUiRefresh = 0;

float tempSetpoint = 37.5;   // Target temperature
float tempHysteresis = 0.3;  // +/- hysteresis band
float humSetpoint = 60.0;    // default humidity target
float humHysteresis = 3.0;

bool humidifierOn = false;
bool heaterOn = false;
bool coolerOn = false;
bool coolerManualOn = false;
bool heaterManualOn = false;

unsigned long lastSensorErrorSent = 0;
unsigned long lastHttpErrorSent = 0;
unsigned long lastHeartbeatSent = 0;


String googleScriptURL =
  "https://script.google.com/macros/s/AKfycbwQPhJOnfiILeU8a0UpUuxf7Jx1jC_9RmE5fsETXEMdrBcwqfmVYo5QaMLl3pi4l369nQ/exec";

void pushError(const char* type, const char* message) {

  ErrorMsg_t err;

  strncpy(err.type, type, sizeof(err.type));
  err.type[sizeof(err.type) - 1] = '\0';

  strncpy(err.message, message, sizeof(err.message));
  err.message[sizeof(err.message) - 1] = '\0';

  xQueueSend(errorQueue, &err, 0);
}
/* ================= TASK: CLOUD ================= */
void task_cloud(void* pvParameters) {

  static unsigned long lastHeartbeat = 0;

  for (;;) {

    /* ================= WIFI CHECK ================= */

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[CLOUD] Reconnecting WiFi...");
      WiFi.reconnect();
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    /* ================= SEND QUEUED ERRORS ================= */

    ErrorMsg_t incomingError;

    while (xQueueReceive(errorQueue, &incomingError, 0) == pdTRUE) {

      WiFiClientSecure client;
      client.setInsecure();

      HTTPClient http;
      http.setTimeout(5000);

      String msg = String(incomingError.message);
      msg.replace(" ", "%20");

      String url = googleScriptURL + "?id=" + String(DEVICE_ID) + "&fw=" + String(FW_VERSION) + "&error=" + String(incomingError.type) + "&msg=" + msg;

      Serial.println("[CLOUD] Sending ERROR log...");

      http.begin(client, url);
      http.GET();
      http.end();
    }

    /* ================= HEARTBEAT ================= */

    if (millis() - lastHeartbeat > 300000) {  // 5 minutes
      pushError("HEARTBEAT", "Device Alive");
      lastHeartbeat = millis();
    }

    /* ================= READ SENSOR DATA ================= */

    float t = 0.0;
    float h = 0.0;

    if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(200))) {
      t = gSensorData.temp_ds18b20;
      h = gSensorData.humidity_dht;
      xSemaphoreGive(sensorMutex);
    }

    /* ================= DATA VALIDATION ================= */

    String tempStr;
    if (t < -100 || t > 100) {
      tempStr = "NA";
    } else {
      tempStr = String(t, 1);
    }

    String humStr;
    if (h < 0 || h > 100) {
      humStr = "NA";
    } else {
      humStr = String((int)h);
    }

    String setTempStr = String(tempSetpoint, 1);
    String setHumStr = String(humSetpoint, 1);


    /* ================= TELEMETRY ================= */

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(5000);

    String url = googleScriptURL + "?id=" + String(DEVICE_ID) + "&fw=" + String(FW_VERSION) + "&temp=" + tempStr + "&hum=" + humStr + "&setTemp=" + setTempStr + "&setHum=" + setHumStr + "&mode=" + String(heaterMode == MODE_AUTO ? "AUTO" : "MANUAL") + "&heater=" + String(heaterOn ? "1" : "0") + "&cooler=" + String(coolerOn ? "1" : "0");

    Serial.println("[CLOUD] Sending telemetry...");

    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode > 0) {

      Serial.print("[CLOUD] HTTP Code: ");
      Serial.println(httpCode);

      if (httpCode != 200 && httpCode != 302) {
        pushError("SERVER_ERROR", "HTTP unexpected");
      }

    } else {

      Serial.println("[CLOUD] HTTP Failed");

      pushError("HTTP_FAIL", "Upload failed");
    }

    http.end();

    /* ================= INTERVAL ================= */

    vTaskDelay(pdMS_TO_TICKS(60000));  // 60 sec
  }
}


void task_temperature_control(void* pvParameters) {

  const float MAX_TEMP_LIMIT = 39.5;  // Absolute safety limit

  for (;;) {

    float currentTemp = 0.0;

    if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
      currentTemp = gSensorData.temp_ds18b20;
      xSemaphoreGive(sensorMutex);
    }

    /* ================= SENSOR SAFETY ================= */

    if (currentTemp < -100 || currentTemp > 100) {

      heaterOn = false;
      humidifierOn = false;

      digitalWrite(RELAY_HEATER, RELAY_OFF);
      digitalWrite(RELAY_HUMIDIFIER, RELAY_OFF);

      Serial.println("[SAFETY] Sensor invalid - All outputs OFF");

      if (millis() - lastSensorErrorSent > 60000) {
        pushError("SENSOR_ERROR", "DS18B20 disconnected");
        lastSensorErrorSent = millis();
      }

      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    /* ================= FAULT LATCH CHECK ================= */

    if (overTempFault) {

      heaterOn = false;
      humidifierOn = false;

      digitalWrite(RELAY_HEATER, RELAY_OFF);
      digitalWrite(RELAY_HUMIDIFIER, RELAY_OFF);

      vTaskDelay(pdMS_TO_TICKS(500));
      continue;  // Skip control logic
    }
    /* ================= OVER-TEMPERATURE SAFETY ================= */

    if (currentTemp > MAX_TEMP_LIMIT) {

      overTempFault = true;
      heaterOn = false;
      humidifierOn = false;

      digitalWrite(RELAY_HEATER, RELAY_OFF);
      digitalWrite(RELAY_HUMIDIFIER, RELAY_OFF);

      Serial.println("[FAULT] OVER TEMPERATURE - LATCHED");

      pushError("OVER_TEMP", "Incubator Over Temperature");
    }
    /* ================= HEATER CONTROL ================= */

    if (!heaterOn && currentTemp <= (tempSetpoint - tempHysteresis)) {

      heaterOn = true;
      digitalWrite(RELAY_HEATER, RELAY_ON);
      Serial.println("[RELAY] HEATER ON");

    } else if (heaterOn && currentTemp >= (tempSetpoint + tempHysteresis)) {

      heaterOn = false;
      digitalWrite(RELAY_HEATER, RELAY_OFF);
      Serial.println("[RELAY] HEATER OFF");
    }

    /* ================= HUMIDITY CONTROL ================= */

    float currentHum = 0.0;

    if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
      currentHum = gSensorData.humidity_dht;
      xSemaphoreGive(sensorMutex);
    }

    if (currentHum >= 0 && currentHum <= 100) {

      if (!humidifierOn && currentHum <= (humSetpoint - humHysteresis)) {

        humidifierOn = true;
        digitalWrite(RELAY_HUMIDIFIER, RELAY_ON);
        Serial.println("[RELAY] HUMIDIFIER ON");

      } else if (humidifierOn && currentHum >= (humSetpoint + humHysteresis)) {

        humidifierOn = false;
        digitalWrite(RELAY_HUMIDIFIER, RELAY_OFF);
        Serial.println("[RELAY] HUMIDIFIER OFF");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

/* ================= TASK: SENSOR ================= */
void task_sensor(void* pvParameters) {
  dht.begin();

  float lastTemp = 0.0;
  float lastHum = 0.0;

  for (;;) {
    //float t = dht.readTemperature();
    float h = dht.readHumidity();

    Serial.print("[DHT] T=");
    //Serial.print(t);
    Serial.print(" H=");
    Serial.println(h);


    //if (isnan(t)) t = lastTemp;
    if (isnan(h)) h = lastHum;

    //lastTemp = t;
    lastHum = h;

    if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
      //gSensorData.temperature = t;
      gSensorData.humidity_dht = h;
      xSemaphoreGive(sensorMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void task_ds18b20(void* pvParameters) {

  ds18b20.begin();
  ds18b20.setWaitForConversion(false);

  for (;;) {

    ds18b20.requestTemperatures();
    vTaskDelay(pdMS_TO_TICKS(800));

    float t = ds18b20.getTempCByIndex(0);

    if (t == DEVICE_DISCONNECTED_C) {

      if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
        gSensorData.temp_ds18b20 = -999.0;  // special error value
        xSemaphoreGive(sensorMutex);
      }

      pushError("SENSOR_ERROR", "DS18B20 disconnected");

    } else {

      if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
        gSensorData.temp_ds18b20 = t;
        xSemaphoreGive(sensorMutex);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


/* ================= TASK: RTC ================= */
void task_rtc(void* pvParameters) {
  for (;;) {
    DateTime now = rtc.now();

    if (xSemaphoreTake(rtcMutex, portMAX_DELAY)) {
      gRtcTime.now = now;
      gRtcTime.epoch = now.unixtime();
      xSemaphoreGive(rtcMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

/* ================= TASK: BUTTONS ================= */
void task_buttons(void* pvParameters) {

  btnUp.setDebounceTime(50);
  btnDown.setDebounceTime(50);
  btnOk.setDebounceTime(50);

  static unsigned long okPressStart = 0;
  static bool okHeld = false;

  for (;;) {

    btnUp.loop();
    btnDown.loop();
    btnOk.loop();

    UiEvent evt;

    /* ================= UP BUTTON ================= */
    if (btnUp.isPressed()) {
      evt = UI_EVT_UP;
      xQueueSend(uiEventQueue, &evt, 0);
    }

    /* ================= DOWN BUTTON ================= */
    if (btnDown.isPressed()) {
      evt = UI_EVT_DOWN;
      xQueueSend(uiEventQueue, &evt, 0);
    }

    /* ================= OK BUTTON ================= */

    // Detect press start
    if (btnOk.isPressed()) {

      okPressStart = millis();
      okHeld = true;

      // Send UI event only if NOT in fault
      if (!overTempFault) {
        evt = UI_EVT_OK;
        xQueueSend(uiEventQueue, &evt, 0);
      }
    }

    // Detect release
    if (btnOk.isReleased()) {

      if (overTempFault && okHeld) {

        // Long press ≥ 3 seconds
        if (millis() - okPressStart >= 3000) {

          overTempFault = false;

          // Ensure outputs remain OFF after reset
          heaterOn = false;
          humidifierOn = false;

          digitalWrite(RELAY_HEATER, RELAY_OFF);
          digitalWrite(RELAY_HUMIDIFIER, RELAY_OFF);

          Serial.println("[FAULT] Manual Reset");
        }
      }

      okHeld = false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

/* ================= TASK: UI ================= */
void task_ui(void* pvParameters) {
  oled_init();

  for (;;) {

    if (overTempFault) {

      float temp = 0.0;

      if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
        temp = gSensorData.temp_ds18b20;
        xSemaphoreGive(sensorMutex);
      }

      oled_show_fault_screen(temp);

      vTaskDelay(pdMS_TO_TICKS(500));
      continue;  // Skip normal UI
    }

    if (uiState == UI_HOME) {

      DateTime now;
      float temp = 0.0;
      float hum = 0.0;

      if (xSemaphoreTake(rtcMutex, portMAX_DELAY)) {
        now = gRtcTime.now;
        xSemaphoreGive(rtcMutex);
      }

      if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
        temp = gSensorData.temp_ds18b20;
        hum = gSensorData.humidity_dht;
        xSemaphoreGive(sensorMutex);
      }

      bool redraw = false;

      if (now.minute() != lastMinute) {
        lastMinute = now.minute();
        redraw = true;
      }

      if (millis() - lastSensorUiUpdate > 3000) {
        lastSensorUiUpdate = millis();
        redraw = true;
      }

      if (redraw) {
        oled_show_home(
          now,
          3,
          temp,
          hum,
          tempSetpoint,
          humSetpoint,
          heaterMode == MODE_AUTO,
          heaterOn,
          coolerOn,
          humidifierOn);
      }
    }

    UiEvent evt;
    if (xQueueReceive(uiEventQueue, &evt, pdMS_TO_TICKS(50))) {

      if (uiState == UI_HOME && evt == UI_EVT_OK) {
        uiState = UI_MAIN_MENU;
        mainMenuIndex = 0;
        lastMenuIndex = -1;
        oled_show_menu(mainMenuIndex);
        lastMenuIndex = mainMenuIndex;
      }

      else if (uiState == UI_MAIN_MENU) {

        if (evt == UI_EVT_UP) {
          mainMenuIndex = (mainMenuIndex - 1 + MENU_COUNT) % MENU_COUNT;
        } else if (evt == UI_EVT_DOWN) {
          mainMenuIndex = (mainMenuIndex + 1) % MENU_COUNT;
        }

        if (mainMenuIndex != lastMenuIndex) {
          oled_show_menu(mainMenuIndex);
          lastMenuIndex = mainMenuIndex;
        }

        if (evt == UI_EVT_OK) {

          if (mainMenuIndex == MENU_CONTROLLER_MODE) {
            uiState = UI_CONTROLLER_MODE_MENU;
            controllerModeIndex = 0;
            lastMenuIndex = -1;
            oled_show_controller_mode(controllerModeIndex);
            lastMenuIndex = controllerModeIndex;
          } else if (mainMenuIndex == MENU_SET_ENVIRONMENT) {
            uiState = UI_SET_ENV_MENU;
            environmentMenuIndex = 0;
            lastMenuIndex = -1;
            oled_show_set_environment(environmentMenuIndex);
            lastMenuIndex = environmentMenuIndex;
          } else if (mainMenuIndex == MENU_EXIT) {
            uiState = UI_HOME;
            lastMinute = -1;
          } else if (mainMenuIndex == MENU_SETTINGS) {
            uiState = UI_SETTINGS_MENU;
            settingsMenuIndex = 0;
            lastMenuIndex = -1;
            oled_show_settings_menu(settingsMenuIndex);
            lastMenuIndex = settingsMenuIndex;
          }
        }
      } else if (uiState == UI_CONTROLLER_MODE_MENU) {

        if (evt == UI_EVT_UP) {
          controllerModeIndex = (controllerModeIndex - 1 + MODE_COUNT + 1) % (MODE_COUNT + 1);
        } else if (evt == UI_EVT_DOWN) {
          controllerModeIndex = (controllerModeIndex + 1) % (MODE_COUNT + 1);
        }

        if (controllerModeIndex != lastMenuIndex) {
          oled_show_controller_mode(controllerModeIndex);
          lastMenuIndex = controllerModeIndex;
        }

        if (evt == UI_EVT_OK) {
          if (controllerModeIndex == MODE_COUNT) {  // Back
            uiState = UI_MAIN_MENU;
            lastMenuIndex = -1;
            oled_show_menu(mainMenuIndex);
            lastMenuIndex = mainMenuIndex;
          }
          // Mode selection logic will come later
        }
      } else if (uiState == UI_SET_ENV_MENU) {

        if (evt == UI_EVT_UP) {
          environmentMenuIndex = (environmentMenuIndex - 1 + ENV_COUNT) % ENV_COUNT;
        } else if (evt == UI_EVT_DOWN) {
          environmentMenuIndex = (environmentMenuIndex + 1) % ENV_COUNT;
        }

        if (environmentMenuIndex != lastMenuIndex) {
          oled_show_set_environment(environmentMenuIndex);
          lastMenuIndex = environmentMenuIndex;
        }

        if (evt == UI_EVT_OK) {
          if (environmentMenuIndex == ENV_TEMPERATURE) {
            uiState = UI_ENV_TEMPERATURE;
            lastMenuIndex = -1;

            float currentTemp = 0.0;
            if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
              currentTemp = gSensorData.temp_ds18b20;
              xSemaphoreGive(sensorMutex);
            }

            oled_show_temperature(currentTemp, tempSetpoint);
          }
          if (environmentMenuIndex == ENV_HYSTERESIS) {

            uiState = UI_ENV_HYSTERESIS_MENU;
            hysteresisMenuIndex = 0;
            lastMenuIndex = -1;

            oled_show_hysteresis_menu(
              hysteresisMenuIndex,
              tempHysteresis,
              humHysteresis);

            lastMenuIndex = hysteresisMenuIndex;
          }

          if (environmentMenuIndex == ENV_HUMIDITY) {

            uiState = UI_ENV_HUMIDITY;

            float currentHum = 0.0;

            if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
              currentHum = gSensorData.humidity_dht;
              xSemaphoreGive(sensorMutex);
            }

            oled_show_humidity(currentHum, humSetpoint);
          }


          if (environmentMenuIndex == ENV_BACK) {
            uiState = UI_MAIN_MENU;
            lastMenuIndex = -1;
            oled_show_menu(mainMenuIndex);
            lastMenuIndex = mainMenuIndex;
          }
          // Temperature / Humidity / Day handled next
        }
      } else if (uiState == UI_SETTINGS_MENU) {

        if (evt == UI_EVT_UP) {
          settingsMenuIndex = (settingsMenuIndex - 1 + SET_COUNT) % SET_COUNT;
        } else if (evt == UI_EVT_DOWN) {
          settingsMenuIndex = (settingsMenuIndex + 1) % SET_COUNT;
        }

        if (settingsMenuIndex != lastMenuIndex) {
          oled_show_settings_menu(settingsMenuIndex);
          lastMenuIndex = settingsMenuIndex;
        }

        if (evt == UI_EVT_OK) {

          // Mode submenu
          if (settingsMenuIndex == SET_MODE) {

            uiState = UI_MODE_MENU;
            modeMenuIndex = 0;
            lastMenuIndex = -1;

            oled_show_mode_menu(modeMenuIndex);
            lastMenuIndex = modeMenuIndex;
          }

          else if (settingsMenuIndex == SET_BACK) {
            uiState = UI_MAIN_MENU;
            lastMenuIndex = -1;
            oled_show_menu(mainMenuIndex);
            lastMenuIndex = mainMenuIndex;
          }
        }
      } else if (uiState == UI_ENV_TEMPERATURE) {

        bool redraw = false;

        // Handle buttons
        if (evt == UI_EVT_UP) {
          tempSetpoint += 0.1;
          redraw = true;
        } else if (evt == UI_EVT_DOWN) {
          tempSetpoint -= 0.1;
          redraw = true;
        }

        // Clamp
        if (tempSetpoint < 30.0) tempSetpoint = 30.0;
        if (tempSetpoint > 45.0) tempSetpoint = 45.0;

        // Periodic refresh (every 1s)
        if (millis() - lastTempUiRefresh > 1000) {
          redraw = true;
          lastTempUiRefresh = millis();
        }

        if (redraw) {
          float currentTemp = 0.0;
          if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
            currentTemp = gSensorData.temp_ds18b20;
            xSemaphoreGive(sensorMutex);
          }

          oled_show_temperature(currentTemp, tempSetpoint);
        }

        if (evt == UI_EVT_OK) {
          saveSettings();
          uiState = UI_SET_ENV_MENU;
          lastMenuIndex = -1;
          oled_show_set_environment(environmentMenuIndex);
          lastMenuIndex = environmentMenuIndex;
        }
      } else if (uiState == UI_ENV_HYSTERESIS) {

        if (evt == UI_EVT_UP) {
          tempHysteresis += 0.1;
        } else if (evt == UI_EVT_DOWN) {
          tempHysteresis -= 0.1;
        }

        // Safety clamp
        if (tempHysteresis < 0.1) tempHysteresis = 0.1;
        if (tempHysteresis > 2.0) tempHysteresis = 2.0;

        oled_show_hysteresis(tempHysteresis);

        if (evt == UI_EVT_OK) {
          saveSettings();
          uiState = UI_SET_ENV_MENU;
          lastMenuIndex = -1;
          oled_show_set_environment(environmentMenuIndex);
          lastMenuIndex = environmentMenuIndex;
        }
      } else if (uiState == UI_ENV_HUMIDITY) {

        bool redraw = false;

        if (evt == UI_EVT_UP) {
          humSetpoint += 1;
          redraw = true;
        } else if (evt == UI_EVT_DOWN) {
          humSetpoint -= 1;
          redraw = true;
        }

        // Clamp range
        if (humSetpoint < 30) humSetpoint = 30;
        if (humSetpoint > 90) humSetpoint = 90;

        if (redraw) {

          float currentHum = 0.0;

          if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
            currentHum = gSensorData.humidity_dht;
            xSemaphoreGive(sensorMutex);
          }

          oled_show_humidity(currentHum, humSetpoint);
        }

        if (evt == UI_EVT_OK) {
          saveSettings();
          uiState = UI_SET_ENV_MENU;
          lastMenuIndex = -1;
          oled_show_set_environment(environmentMenuIndex);
          lastMenuIndex = environmentMenuIndex;
        }
      } else if (uiState == UI_MODE_MENU) {

        if (evt == UI_EVT_UP) {
          modeMenuIndex = (modeMenuIndex - 1 + 3) % 3;
        } else if (evt == UI_EVT_DOWN) {
          modeMenuIndex = (modeMenuIndex + 1) % 3;
        }

        if (modeMenuIndex != lastMenuIndex) {
          oled_show_mode_menu(modeMenuIndex);
          lastMenuIndex = modeMenuIndex;
        }

        if (evt == UI_EVT_OK) {

          if (modeMenuIndex == 0) {  // AUTO

            heaterMode = MODE_AUTO;
            saveSettings();
            uiState = UI_SETTINGS_MENU;
            lastMenuIndex = -1;
            oled_show_settings_menu(settingsMenuIndex);
            lastMenuIndex = settingsMenuIndex;
          }

          else if (modeMenuIndex == 1) {  // MANUAL

            heaterMode = MODE_MANUAL;
            saveSettings();
            uiState = UI_MANUAL_CONTROL_MENU;
            manualControlIndex = 0;
            lastMenuIndex = -1;

            oled_show_manual_control(
              manualControlIndex,
              heaterManualOn,
              coolerManualOn);

            lastMenuIndex = manualControlIndex;
          }

          else if (modeMenuIndex == 2) {  // BACK

            uiState = UI_SETTINGS_MENU;
            lastMenuIndex = -1;
            oled_show_settings_menu(settingsMenuIndex);
            lastMenuIndex = settingsMenuIndex;
          }
        }
      } else if (uiState == UI_MANUAL_CONTROL_MENU) {

        if (evt == UI_EVT_UP) {
          manualControlIndex = (manualControlIndex - 1 + 3) % 3;
        } else if (evt == UI_EVT_DOWN) {
          manualControlIndex = (manualControlIndex + 1) % 3;
        }

        if (manualControlIndex != lastMenuIndex) {
          oled_show_manual_control(
            manualControlIndex,
            heaterManualOn,
            coolerManualOn);
          lastMenuIndex = manualControlIndex;
        }

        if (evt == UI_EVT_OK) {

          if (manualControlIndex == 0) {  // HEATER
            heaterManualOn = !heaterManualOn;
            saveSettings();
          }

          else if (manualControlIndex == 1) {  // COOLER
            coolerManualOn = !coolerManualOn;
            saveSettings();
          }

          else if (manualControlIndex == 2) {  // BACK
            uiState = UI_MODE_MENU;
            lastMenuIndex = -1;
            oled_show_mode_menu(modeMenuIndex);
            lastMenuIndex = modeMenuIndex;
            continue;  // IMPORTANT
          }


          oled_show_manual_control(
            manualControlIndex,
            heaterManualOn,
            coolerManualOn);
        }
      } else if (uiState == UI_ENV_HYSTERESIS_MENU) {

        if (evt == UI_EVT_UP) {
          hysteresisMenuIndex = (hysteresisMenuIndex - 1 + 3) % 3;
        } else if (evt == UI_EVT_DOWN) {
          hysteresisMenuIndex = (hysteresisMenuIndex + 1) % 3;
        }

        if (hysteresisMenuIndex != lastMenuIndex) {
          oled_show_hysteresis_menu(
            hysteresisMenuIndex,
            tempHysteresis,
            humHysteresis);
          lastMenuIndex = hysteresisMenuIndex;
        }

        if (evt == UI_EVT_OK) {

          if (hysteresisMenuIndex == 0) {
            uiState = UI_ENV_HYST_TEMP_EDIT;
          } else if (hysteresisMenuIndex == 1) {
            uiState = UI_ENV_HYST_HUM_EDIT;
          } else if (hysteresisMenuIndex == 2) {
            saveSettings();
            uiState = UI_SET_ENV_MENU;
            lastMenuIndex = -1;
            oled_show_set_environment(environmentMenuIndex);
            lastMenuIndex = environmentMenuIndex;
            continue;
          }
        }
      } else if (uiState == UI_ENV_HYST_TEMP_EDIT) {

        if (evt == UI_EVT_UP) {
          tempHysteresis += 0.1;
          if (tempHysteresis > 2.0) tempHysteresis = 2.0;
        } else if (evt == UI_EVT_DOWN) {
          tempHysteresis -= 0.1;
          if (tempHysteresis < 0.1) tempHysteresis = 0.1;
        }

        oled_show_hysteresis_menu(
          hysteresisMenuIndex,
          tempHysteresis,
          humHysteresis);

        if (evt == UI_EVT_OK) {
          saveSettings();
          uiState = UI_ENV_HYSTERESIS_MENU;
        }
      } else if (uiState == UI_ENV_HYST_HUM_EDIT) {

        if (evt == UI_EVT_UP) {
          humHysteresis += 1;
          if (humHysteresis > 10) humHysteresis = 10;
        } else if (evt == UI_EVT_DOWN) {
          humHysteresis -= 1;
          if (humHysteresis < 1) humHysteresis = 1;
        }

        oled_show_hysteresis_menu(
          hysteresisMenuIndex,
          tempHysteresis,
          humHysteresis);

        if (evt == UI_EVT_OK) {
          saveSettings();
          uiState = UI_ENV_HYSTERESIS_MENU;
        }
      }
    }
  }
}
void loadSettings() {

  prefs.begin("incubator", true);  // read only

  tempSetpoint = prefs.getFloat("setTemp", 37.5);
  tempHysteresis = prefs.getFloat("hyst", 0.3);
  heaterMode = (ControlMode)prefs.getUInt("mode", MODE_AUTO);
  heaterManualOn = prefs.getBool("heatMan", false);
  coolerManualOn = prefs.getBool("coolMan", false);
  humSetpoint = prefs.getFloat("setHum", 60.0);
  humHysteresis = prefs.getFloat("humHyst", 3.0);


  prefs.end();

  Serial.println("[NVS] Settings Loaded");
}

void saveSettings() {

  prefs.begin("incubator", false);  // write mode

  prefs.putFloat("setTemp", tempSetpoint);
  prefs.putFloat("hyst", tempHysteresis);
  prefs.putUInt("mode", heaterMode);
  prefs.putBool("heatMan", heaterManualOn);
  prefs.putBool("coolMan", coolerManualOn);
  prefs.putFloat("setHum", humSetpoint);
  prefs.putFloat("humHyst", humHysteresis);


  prefs.end();

  Serial.println("[NVS] Settings Saved");
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  WiFiManager wm;

  Serial.println("[SETUP] Starting WiFiManager...");

  if (!wm.autoConnect("INCUBATOR_SETUP")) {
    Serial.println("[SETUP] Failed to connect. Restarting...");
    ESP.restart();
  }

  Serial.println("[SETUP] WiFi Connected!");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());


  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_COOLER, OUTPUT);
  pinMode(RELAY_HUMIDIFIER, OUTPUT);



  // Safety: OFF at boot
  digitalWrite(RELAY_HEATER, RELAY_OFF);
  digitalWrite(RELAY_COOLER, RELAY_OFF);
  digitalWrite(RELAY_HUMIDIFIER, RELAY_OFF);


  Wire.begin(I2C_SDA, I2C_SCL);

  if (!rtc.begin()) {
    while (1)
      ;
  }

  rtcMutex = xSemaphoreCreateMutex();
  sensorMutex = xSemaphoreCreateMutex();
  uiEventQueue = xQueueCreate(10, sizeof(UiEvent));
  errorQueue = xQueueCreate(10, sizeof(ErrorMsg_t));

  loadSettings();
  if (heaterMode == MODE_MANUAL) {
    heaterOn = heaterManualOn;
    coolerOn = coolerManualOn;
  }

  if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
    gSensorData.temp_ds18b20 = 0.0;
    gSensorData.humidity_dht = 0.0;
    xSemaphoreGive(sensorMutex);
  }

  xTaskCreatePinnedToCore(task_rtc, "RTC", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(task_buttons, "Buttons", 2048, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(task_ui, "UI", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(task_sensor, "Sensor", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(task_ds18b20, "DS18B20", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(task_cloud, "Cloud", 10240, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(task_temperature_control, "TempCtrl", 2048, NULL, 2, NULL, 1);
}

void loop() {
  /* RTOS only */
}
