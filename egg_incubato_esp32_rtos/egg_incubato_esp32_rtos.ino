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


/* ================= PINS ================= */
#define BTN_UP 32
#define BTN_DOWN 33
#define BTN_OK 25

#define I2C_SDA 21
#define I2C_SCL 22

#define DHT_PIN 4
#define DHT_TYPE DHT11

#define DS18B20_PIN 18

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
  UI_ENV_TURNING
};


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

UiState uiState = UI_HOME;
int mainMenuIndex = 0;
int controllerModeIndex = 0;
int environmentMenuIndex = 0;
int settingsMenuIndex = 0;
int lastMenuIndex = -1;
int lastMinute = -1;
unsigned long lastSensorUiUpdate = 0;
unsigned long lastTempUiRefresh = 0;
float tempSetpoint = 37.5;   // Target temperature
float tempHysteresis = 0.3;  // +/- hysteresis band
bool heaterOn = false;



String googleScriptURL =
  "https://script.google.com/macros/s/AKfycbzwaA2LvXgwjS5uKyzoz7kOgmiPWyOK5AIbWJ_js8-MqJfCbuuzWJ-_5fo4K0R49e8k-g/exec";

/* ================= TASK: CLOUD ================= */
void task_cloud(void* pvParameters) {
  WiFiManager wm;

  Serial.println("[CLOUD] Starting WiFiManager");

  bool res = wm.autoConnect("INCUBATOR_SETUP");
  if (!res) {
    Serial.println("[CLOUD] WiFi FAILED");
    vTaskDelete(NULL);
  }

  Serial.println("[CLOUD] WiFi CONNECTED");
  Serial.print("[CLOUD] IP: ");
  Serial.println(WiFi.localIP());

  for (;;) {
    float t, h;

    if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
      t = gSensorData.temp_ds18b20;
      h = gSensorData.humidity_dht;
      xSemaphoreGive(sensorMutex);
    }

    if (WiFi.status() == WL_CONNECTED) {
      WiFiClientSecure client;
      client.setInsecure();

      HTTPClient http;
      String url = googleScriptURL + "?temp=" + String(t, 1) + "&hum=" + String((int)h);

      http.begin(client, url);
      int httpCode = http.GET();

      Serial.print("[CLOUD] URL: ");
      Serial.println(url);
      Serial.print("[CLOUD] HTTP code: ");
      Serial.println(httpCode);

      http.end();
    }

    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}

void task_temperature_control(void* pvParameters) {

  for (;;) {
    float currentTemp = 0.0;

    if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
      currentTemp = gSensorData.temp_ds18b20;
      xSemaphoreGive(sensorMutex);
    }

    // Hysteresis control
    if (!heaterOn && currentTemp <= (tempSetpoint - tempHysteresis)) {
      heaterOn = true;
      Serial.println("[CTRL] Heater ON");
    } else if (heaterOn && currentTemp >= (tempSetpoint + tempHysteresis)) {
      heaterOn = false;
      Serial.println("[CTRL] Heater OFF");
    }

    // Later: digitalWrite(HEATER_PIN, heaterOn);

    vTaskDelay(pdMS_TO_TICKS(500));  // control loop
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
        oled_show_home(now, 3, temp, hum);
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
          uiState = UI_SET_ENV_MENU;
          lastMenuIndex = -1;
          oled_show_set_environment(environmentMenuIndex);
          lastMenuIndex = environmentMenuIndex;
        }
      }
    }
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!rtc.begin()) {
    while (1)
      ;
  }

  rtcMutex = xSemaphoreCreateMutex();
  sensorMutex = xSemaphoreCreateMutex();
  uiEventQueue = xQueueCreate(10, sizeof(UiEvent));

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
