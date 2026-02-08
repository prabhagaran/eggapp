#include "oled_ui.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define OLED_ADDR    0x3C

#define I2C_SDA 21
#define I2C_SCL 22

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void oled_init(void)
{
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1);
  }

  display.clearDisplay();
  display.display();
}

void oled_show_home(const DateTime& now, int day, float temp, float hum)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  /* -------- Top-right Date -------- */
  char dateStr[6];
  snprintf(dateStr, sizeof(dateStr), "%02d-%02d", now.day(), now.month());
  display.setCursor(128 - (strlen(dateStr) * 6), 0);
  display.print(dateStr);

  /* -------- Top-right Time -------- */
  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", now.hour(), now.minute());
  display.setCursor(128 - (strlen(timeStr) * 6), 8);
  display.print(timeStr);

  /* -------- Body -------- */
  display.setCursor(0, 24);
  display.printf("Day : %02d / 21\n", day);
  display.printf("Temp: %.1f C\n", temp);
  display.printf("Hum : %.0f %%\n", hum);

  display.display();
}

void oled_show_menu(int selected)
{
  static const char* menuItems[] = {
    "Time & Date",
    "Incubation Day",
    "Settings",
    "Exit"
  };

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("MAIN MENU");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  for (int i = 0; i < 4; i++) {
    display.setCursor(0, 20 + i * 10);
    display.print(i == selected ? "> " : "  ");
    display.println(menuItems[i]);
  }

  display.display();
}
