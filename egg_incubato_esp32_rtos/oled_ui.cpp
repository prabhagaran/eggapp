#include "oled_ui.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define OLED_ADDR    0x3C

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void oled_init(void)
{
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1);
  }

  display.clearDisplay();
  display.display();
}

void oled_show_boot(void)
{
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("EGG INCUBATOR");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  display.setCursor(0, 25);
  display.println("System Booting...");
  display.println("OLED Ready");

  display.display();
}

void oled_show_status(float temp, float hum, int day, bool heater, bool wifi)
{
  display.clearDisplay();

  // Header (yellow)
  display.setCursor(0, 0);
  display.println("INCUBATION RUN");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  // Body (blue)
  display.setCursor(0, 20);
  display.printf("Temp : %.1f C\n", temp);
  display.printf("Hum  : %.0f %%\n", hum);
  display.printf("Day  : %02d / 21\n", day);
  display.printf("Heat : %s\n", heater ? "ON" : "OFF");
  display.printf("WiFi : %s\n", wifi ? "OK" : "OFF");

  display.display();
}
