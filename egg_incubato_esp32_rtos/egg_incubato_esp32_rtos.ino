#include <Arduino.h>
#include <Wire.h>
#include <ezButton.h>
#include "RTClib.h"
#include "oled_ui.h"
#include <DHT.h>

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
/* ================= SHARED RTC DATA ================= */
typedef struct {
  float temperature;
  float humidity;
} SensorData_t;

SensorData_t gSensorData;
SemaphoreHandle_t sensorMutex;

/* ================= SHARED RTC DATA ================= */
typedef struct {
  DateTime now;
  uint32_t epoch;
} RtcTime_t;

RtcTime_t gRtcTime;
SemaphoreHandle_t rtcMutex;

/* ================= GLOBALS ================= */
QueueHandle_t uiEventQueue;
RTC_DS1307 rtc;

/* ================= BUTTON OBJECTS ================= */
ezButton btnUp(BTN_UP);
ezButton btnDown(BTN_DOWN);
ezButton btnOk(BTN_OK);

/* ================= UI STATE ================= */
UiState uiState = UI_HOME;
int menuIndex = 0;
int lastMenuIndex = -1;
int lastMinute = -1;

String googleScriptURL =
  "https://script.google.com/macros/s/AKfycbw9pKv0ojIoN3bpq8qJGAdbnDf4E-kDDfy8Y36-KIDzUr3MVwG8Mlta-2ozrXsegSOBcQ/exec";

void task_cloud(void* pvParameters) {
  WiFiManager wm;

  bool res = wm.autoConnect("INCUBATOR_SETUP");
  if (!res) {
    vTaskDelete(NULL);
  }

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

      String url = googleScriptURL + "?temp=" + String(t, 1) + "&hum=" + String(h, 0);

      http.begin(client, url);
      http.GET();
      http.end();
    }

    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}


void task_sensor(void* pvParameters) {
  dht.begin();

  for (;;) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      if (xSemaphoreTake(sensorMutex, portMAX_DELAY)) {
        gSensorData.temperature = t;
        gSensorData.humidity = h;
        xSemaphoreGive(sensorMutex);
      }
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

    /* ----- HOME PAGE UPDATE ----- */
    /* ----- HOME PAGE UPDATE ----- */
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

      if (now.minute() != lastMinute) {
        lastMinute = now.minute();

        oled_show_home(
          now,
          3,  // incubation day (to be replaced)
          temp,
          hum);
      }
    }

    /* ----- HANDLE UI EVENTS ----- */
    UiEvent evt;
    if (xQueueReceive(uiEventQueue, &evt, pdMS_TO_TICKS(50))) {

      if (uiState == UI_HOME) {
        if (evt == UI_EVT_OK) {
          uiState = UI_MENU;
          menuIndex = 0;
          lastMenuIndex = -1;

          oled_show_menu(menuIndex);
          lastMenuIndex = menuIndex;
        }
      }

      else if (uiState == UI_MENU) {

        if (evt == UI_EVT_UP) {
          menuIndex--;
          if (menuIndex < 0)
            menuIndex = MENU_COUNT - 1;
        }

        else if (evt == UI_EVT_DOWN) {
          menuIndex++;
          if (menuIndex >= MENU_COUNT)
            menuIndex = 0;
        }

        if (menuIndex != lastMenuIndex) {
          oled_show_menu(menuIndex);
          lastMenuIndex = menuIndex;
        }

        if (evt == UI_EVT_OK) {

          if (menuIndex == MENU_TIME) {
            uiState = UI_TIME;
          } else if (menuIndex == MENU_DAY) {
            uiState = UI_DAY;
          } else if (menuIndex == MENU_SETTINGS) {
            uiState = UI_SETTINGS;
          } else if (menuIndex == MENU_EXIT) {
            uiState = UI_HOME;
            lastMinute = -1;
          }
        }
      }

      else if (uiState == UI_TIME || uiState == UI_DAY || uiState == UI_SETTINGS) {

        if (evt == UI_EVT_OK) {
          uiState = UI_MENU;
          lastMenuIndex = -1;
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
  uiEventQueue = xQueueCreate(10, sizeof(UiEvent));
  sensorMutex = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(task_rtc, "RTC", 2048, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(task_buttons, "Buttons", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(task_ui, "UI", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(task_sensor, "Sensor", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(task_cloud, "Cloud", 4096, NULL, 1, NULL, 1);
}

void loop() {
  /* RTOS only */
}
