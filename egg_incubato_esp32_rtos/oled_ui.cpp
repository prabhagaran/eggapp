#include "oled_ui.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

#define I2C_SDA 21
#define I2C_SCL 22



static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void oled_init(void) {
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1)
      ;
  }

  display.clearDisplay();
  display.display();
}

void oled_show_home(
  const DateTime& now,
  int day,
  float temp,
  float hum
)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  /* ---------- TOP AREA (YELLOW) ---------- */

  /* Status (left) */
  display.setCursor(0, 0);
  display.print("RUNNING");

  /* Time (right) */
  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", now.hour(), now.minute());
  display.setCursor(88, 0);
  display.print(timeBuf);

  /* Day (left) */
  display.setCursor(0, 8);          // MUST stay <= 8
  display.print("Day ");
  display.print(day);
  display.print(" / 21");

  /* Date (right) */
  static const char* MONTH_NAMES[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };

  char dateBuf[12];
  sprintf(dateBuf, "%02d %s",
          now.day(),
          MONTH_NAMES[now.month() - 1]);

  display.setCursor(88, 8);          // SAME Y as Day
  display.print(dateBuf);

  /* Divider line (last yellow row) */
  display.drawLine(0, 16, 127, 16, SSD1306_WHITE);

  /* ---------- LOWER AREA (BLUE REGION) ---------- */

  /* Temp label */
  display.setCursor(0, 24);
  display.print("Temp:");

  /* Temp value */
  display.setCursor(30, 24);
  display.print(temp, 1);
  display.print(" C");

  /* Hum label */
  display.setCursor(0, 36);
  display.print("Hum :");

  /* Hum value */
  display.setCursor(30, 36);
  display.print((int)hum);
  display.print(" %");

  /* Menu (bottom-left) */
  display.setCursor(0, 56);
  display.print("Menu");

  display.display();
}



void oled_show_menu(int selected) {
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
