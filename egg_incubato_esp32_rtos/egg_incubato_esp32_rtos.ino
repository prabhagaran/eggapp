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

/* ================= PINS ================= */
#define BTN_UP 32
#define BTN_DOWN 33
#define BTN_OK 25

#define I2C_SDA 21
#define I2C_SCL 22

#define DHT_PIN 4
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);

/* ================= UI EVENTS ================= */
enum UiEvent {
  UI_EVT_UP,
  UI_EVT_DOWN,
  UI_EVT_OK
};

/* ================= MENU ================= */
enum MenuItem {
  MENU_TIME = 0,
  MENU_DAY,
  MENU_SETTINGS,
  MENU_EXIT,
  MENU_COUNT
};

/* ================= UI STATES ================= */
enum UiState {
  UI_HOME,
  UI_MENU,
  UI_TIME,
  UI_DAY,
  UI_SETTINGS
};

/* ================= SENSOR DATA ================= */
typedef struct {
  float temperature;
  float humidity;
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
int menuIndex = 0;
int lastMenuIndex = -1;
int lastMinute = -1;
unsigned long lastSensorUiUpdate = 0;

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
      t = gSensorData.temperature;
      h = gSensorData.humidity;
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

/* ================= TASK: SENSOR ================= */
void task_sensor(void* pvParameters) {
  dht.begin();

  float lastTemp = 0.0;
  float lastHum = 0.0;

  for (;;) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    Serial.print("[DHT] T=");
    Serial.print(t);
    Serial.print(" H=");
    Serial.println(h);


    if (isnan(t)) t = lastTemp;
    if (isnan(h)) h = lastHum;

    lastTemp = t;
    lastHum = h;

    if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
      gSensorData.temperature = t;
      gSensorData.humidity = h;
      xSemaphoreGive(sensorMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
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
        temp = gSensorData.temperature;
        hum = gSensorData.humidity;
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
        uiState = UI_MENU;
        menuIndex = 0;
        lastMenuIndex = -1;
        oled_show_menu(menuIndex);
        lastMenuIndex = menuIndex;
      }

      else if (uiState == UI_MENU) {

        if (evt == UI_EVT_UP) {
          menuIndex = (menuIndex - 1 + MENU_COUNT) % MENU_COUNT;
        } else if (evt == UI_EVT_DOWN) {
          menuIndex = (menuIndex + 1) % MENU_COUNT;
        }

        if (menuIndex != lastMenuIndex) {
          oled_show_menu(menuIndex);
          lastMenuIndex = menuIndex;
        }

        if (evt == UI_EVT_OK) {
          if (menuIndex == MENU_EXIT) {
            uiState = UI_HOME;
            lastMinute = -1;
          }
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
    gSensorData.temperature = 0.0;
    gSensorData.humidity = 0.0;
    xSemaphoreGive(sensorMutex);
  }

  xTaskCreatePinnedToCore(task_rtc, "RTC", 2048, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(task_buttons, "Buttons", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(task_ui, "UI", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(task_sensor, "Sensor", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(task_cloud, "Cloud", 10240, NULL, 1, NULL, 1);
}

void loop() {
  /* RTOS only */
}
