#include <Arduino.h>
#include <Wire.h>
#include <ezButton.h>
#include "RTClib.h"
#include "oled_ui.h"

/* ================= PINS ================= */
#define BTN_UP 32
#define BTN_DOWN 33
#define BTN_OK 25

#define I2C_SDA 21
#define I2C_SCL 22

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

    /* ----- HOME PAGE AUTO UPDATE ----- */
    if (uiState == UI_HOME) {
      DateTime now = rtc.now();

      if (now.minute() != lastMinute) {
        lastMinute = now.minute();

        oled_show_home(
          now,
          3,     // dummy day
          37.5,  // dummy temp
          55.0   // dummy hum
        );
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

  uiEventQueue = xQueueCreate(10, sizeof(UiEvent));

  xTaskCreatePinnedToCore(
    task_buttons,
    "Buttons",
    2048,
    NULL,
    2,
    NULL,
    1);

  xTaskCreatePinnedToCore(
    task_ui,
    "UI",
    4096,
    NULL,
    1,
    NULL,
    1);
}

void loop() {
  /* Empty – RTOS runs everything */
}
