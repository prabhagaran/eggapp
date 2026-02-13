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

unsigned long lastSensorUiUpdate = 0;
Preferences prefs;


unsigned long lastTempUiRefresh = 0;
float tempSetpoint = 37.5;   // Target temperature
float tempHysteresis = 0.3;  // +/- hysteresis band

bool heaterOn = false;
bool coolerOn = false;
bool coolerManualOn = false;
bool heaterManualOn = false;
unsigned long lastErrorSent = 0;




String googleScriptURL =
  "https://script.google.com/macros/s/AKfycbzhHZv2t5gtTAkxTMoRq5QyXhRFlbQDU6dIhVVZs96QpcsA07PoNkDlY_e2qM5Clihnmg/exec";

void sendErrorToCloud(String type, String message) {

  if (millis() - lastErrorSent < 60000) {
    Serial.println("[CLOUD] Error skipped (rate limit)");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[CLOUD] Error skipped (WiFi not connected)");
    return;
  }

  lastErrorSent = millis();

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(5000);

  String url = googleScriptURL + "?id=" + String(DEVICE_ID)
               + "&fw=" + String(FW_VERSION)
               + "&error=" + type
               + "&msg=" + message;

  Serial.println("[CLOUD] Sending ERROR:");
  Serial.println(url);

  http.begin(client, url);
  int code = http.GET();

  Serial.print("[CLOUD] Error HTTP Code: ");
  Serial.println(code);

  http.end();
}


/* ================= TASK: CLOUD ================= */
void task_cloud(void* pvParameters) {

  WiFiManager wm;

  Serial.println("[CLOUD] Starting WiFiManager");

  if (!wm.autoConnect("INCUBATOR_SETUP")) {
    Serial.println("[CLOUD] WiFi Failed");
    vTaskDelete(NULL);
  }

  Serial.println("[CLOUD] WiFi Connected");
  static unsigned long lastHeartbeat = 0;

  for (;;) {

    // Ensure WiFi is connected
    if (WiFi.status() != WL_CONNECTED) {

      Serial.println("[CLOUD] Reconnecting WiFi...");
      WiFi.reconnect();

      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 10000) {  // 5 minutes
      sendErrorToCloud("HEARTBEAT", "Device Alive");
      lastHeartbeat = millis();
    }


    float t = 0.0;
    float h = 0.0;

    if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(200))) {
      t = gSensorData.temp_ds18b20;
      h = gSensorData.humidity_dht;
      xSemaphoreGive(sensorMutex);
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(5000);  // 5 second timeout

    String url = googleScriptURL + "?id=" + String(DEVICE_ID) + "&fw=" + String(FW_VERSION) + "&temp=" + String(t, 1) + "&hum=" + String((int)h) + "&mode=" + String(heaterMode == MODE_AUTO ? "AUTO" : "MANUAL") + "&heater=" + String(heaterOn ? "1" : "0") + "&cooler=" + String(coolerOn ? "1" : "0");


    Serial.println("[CLOUD] Sending data...");

    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode > 0) {

      Serial.print("[CLOUD] HTTP Code: ");
      Serial.println(httpCode);

      if (httpCode == 200 || httpCode == 302) {
        Serial.println("[CLOUD] Upload Success");
      } else {
        Serial.println("[CLOUD] Server Error");
        sendErrorToCloud("SERVER_ERROR", "HTTP code unexpected");
      }

    } else {
      Serial.println("[CLOUD] HTTP Failed");
      sendErrorToCloud("HTTP_FAIL", "Upload failed");
    }





    http.end();

    vTaskDelay(pdMS_TO_TICKS(60000));  // 60 sec interval
  }
}


void task_temperature_control(void* pvParameters) {

  for (;;) {

    float currentTemp = 0.0;

    if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
      currentTemp = gSensorData.temp_ds18b20;
      xSemaphoreGive(sensorMutex);
    }

    /* ================= SENSOR SAFETY ================= */

    if (currentTemp <= 0.0 || currentTemp > 100.0) {

      heaterOn = false;
      coolerOn = false;

      digitalWrite(RELAY_HEATER, RELAY_OFF);
      digitalWrite(RELAY_COOLER, RELAY_OFF);

      Serial.println("[SAFETY] Sensor invalid - All outputs OFF");

      sendErrorToCloud("SENSOR_ERROR", "Invalid temperature reading");

      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }


    /* ================= MANUAL MODE ================= */

    if (heaterMode == MODE_MANUAL) {

      heaterOn = heaterManualOn;
      coolerOn = coolerManualOn;

      digitalWrite(RELAY_HEATER,
                   heaterManualOn ? RELAY_ON : RELAY_OFF);

      digitalWrite(RELAY_COOLER,
                   coolerManualOn ? RELAY_ON : RELAY_OFF);

      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }


    /* ================= AUTO MODE ================= */

    float tempHighLimit = tempSetpoint + 1.5;

    // ---- HEATER CONTROL (Hysteresis) ----
    if (!heaterOn && currentTemp <= (tempSetpoint - tempHysteresis)) {

      heaterOn = true;
      coolerOn = false;

      digitalWrite(RELAY_HEATER, RELAY_ON);
      digitalWrite(RELAY_COOLER, RELAY_OFF);

      Serial.println("[RELAY] HEATER ON");
    } else if (heaterOn && currentTemp >= (tempSetpoint + tempHysteresis)) {

      heaterOn = false;

      digitalWrite(RELAY_HEATER, RELAY_OFF);

      Serial.println("[RELAY] HEATER OFF");
    }

    // ---- COOLING SAFETY ----
    if (currentTemp >= tempHighLimit) {

      coolerOn = true;
      heaterOn = false;

      digitalWrite(RELAY_COOLER, RELAY_ON);
      digitalWrite(RELAY_HEATER, RELAY_OFF);

      Serial.println("[RELAY] COOLER ON (HIGH TEMP)");
    } else {
      coolerOn = false;
      digitalWrite(RELAY_COOLER, RELAY_OFF);
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

    if (t != DEVICE_DISCONNECTED_C) {
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

  for (;;) {
    btnUp.loop();
    btnDown.loop();
    btnOk.loop();

    UiEvent evt;

    if (btnUp.isPressed()) {
      evt = UI_EVT_UP;
      xQueueSend(uiEventQueue, &evt, 0);
    }

    if (btnDown.isPressed()) {
      evt = UI_EVT_DOWN;
      xQueueSend(uiEventQueue, &evt, 0);
    }

    if (btnOk.isPressed()) {
      evt = UI_EVT_OK;
      xQueueSend(uiEventQueue, &evt, 0);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

/* ================= TASK: UI ================= */
void task_ui(void* pvParameters) {
  oled_init();

  for (;;) {

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
          heaterMode == MODE_AUTO,
          heaterOn,
          coolerOn);
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
            uiState = UI_ENV_HYSTERESIS;
            oled_show_hysteresis(tempHysteresis);
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

  prefs.end();

  Serial.println("[NVS] Settings Saved");
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_COOLER, OUTPUT);

  // Safety: OFF at boot
  digitalWrite(RELAY_HEATER, RELAY_OFF);
  digitalWrite(RELAY_COOLER, RELAY_OFF);


  Wire.begin(I2C_SDA, I2C_SCL);

  if (!rtc.begin()) {
    while (1)
      ;
  }

  rtcMutex = xSemaphoreCreateMutex();
  sensorMutex = xSemaphoreCreateMutex();
  uiEventQueue = xQueueCreate(10, sizeof(UiEvent));
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
  xTaskCreatePinnedToCore(task_ds18b20, "DS18B20", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(task_cloud, "Cloud", 10240, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(task_temperature_control, "TempCtrl", 2048, NULL, 2, NULL, 1);
}

void loop() {
  /* RTOS only */
}
