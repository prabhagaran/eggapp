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

void oled_show_home(const DateTime& now,
                    int day,
                    float temp,
                    float hum,
                    float tempSetpoint,
                    float humSetpoint,
                    bool isAutoMode,
                    bool heaterOn,
                    bool coolerOn,
                    bool humidifierOn){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  /* ---------- TOP STATUS AREA ---------- */

  display.setCursor(0, 0);
  display.print(isAutoMode ? "AUTO" : "MANUAL");

  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", now.hour(), now.minute());
  display.setCursor(88, 0);
  display.print(timeBuf);
  display.setCursor(0, 8);
  display.print("Day ");
  display.print(day);
  display.print(" / 21");

  static const char* MONTH_NAMES[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };

  char dateBuf[12];
  sprintf(dateBuf, "%02d %s",
          now.day(),
          MONTH_NAMES[now.month() - 1]);

  display.setCursor(88, 8);
  display.print(dateBuf);


  display.drawLine(0, 16, 127, 16, SSD1306_WHITE);

  /* ---------- TEMPERATURE ---------- */

  display.setCursor(0, 22);
  display.print("Temp: ");
  display.print(temp, 1);
  display.print(" / ");
  display.print(tempSetpoint, 1);
  display.print(" C");

  /* ---------- HUMIDITY ---------- */

  display.setCursor(0, 34);
  display.print("Hum : ");
  display.print((int)hum);
  display.print(" / ");
  display.print((int)humSetpoint);
  display.print(" %");


  /* ---------- OUTPUT STATES ---------- */

  display.setCursor(0, 46);
  display.print("Heat: ");
  display.print(heaterOn ? "ON " : "OFF");

  display.setCursor(64, 46);
  display.print("Cool: ");
  display.print(coolerOn ? "ON" : "OFF");

  display.setCursor(0, 56);
  display.print("Humid: ");
  display.print(humidifierOn ? "ON" : "OFF");



  display.display();
}
void oled_show_humidity(float current, float setpoint) {

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("HUMIDITY");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  display.setCursor(0, 24);
  display.print("Current : ");
  display.print((int)current);
  display.print(" %");

  display.setCursor(0, 36);
  display.print("Setpoint: ");
  display.print((int)setpoint);
  display.print(" %");

  display.display();
}
void oled_show_menu(int selected) {
  static const char* menuItems[] = {
    "Controller Mode",
    "Set Environment",
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
void oled_show_controller_mode(int selected) {
  static const char* items[] = {
    "Egg Incubator",
    "Climate Chamber",
    "Thermostat",
    "Common Controller",
    "Back"
  };

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("CONTROLLER MODE");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  for (int i = 0; i < 5; i++) {
    display.setCursor(0, 20 + i * 9);
    display.print(i == selected ? "> " : "  ");
    display.println(items[i]);
  }

  display.display();
}
void oled_show_set_environment(int selected) {
  static const char* items[] = {
    "Temperature",
    "Hysteresis",
    "Humidity",
    "Incubation Day",
    "Turning",
    "Back"
  };


  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("SET ENVIRONMENT");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  for (int i = 0; i < 6; i++) {
    display.setCursor(0, 20 + i * 9);
    display.print(i == selected ? "> " : "  ");
    display.println(items[i]);
  }

  display.display();
}
void oled_show_temperature(float current, float setpoint) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("TEMPERATURE");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  display.setCursor(0, 24);
  display.print("Current : ");
  display.print(current, 1);
  display.print(" C");

  display.setCursor(0, 36);
  display.print("Setpoint: ");
  display.print(setpoint, 1);
  display.print(" C");

  display.display();
}
void oled_show_hysteresis(float hysteresis) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("HYSTERESIS");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  display.setCursor(0, 30);
  display.print("Band : +-");
  display.print(hysteresis, 1);
  display.print(" C");

  display.display();
}
void oled_show_settings_menu(int selected) {
  static const char* items[] = {
    "Time & Date",
    "WiFi",
    "Heater Control",
    "Device Info",
    "Factory Reset",
    "Back"
  };

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("SETTINGS");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  for (int i = 0; i < 6; i++) {
    display.setCursor(0, 20 + i * 8);
    display.print(i == selected ? "> " : "  ");
    display.println(items[i]);
  }

  display.display();
}
void oled_show_heater_control(bool isAutoMode) {

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("HEATER CONTROL");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  display.setCursor(0, 26);
  display.print("Mode : ");
  display.println(isAutoMode ? "AUTO" : "MANUAL");

  display.setCursor(0, 44);
  display.println("OK : Toggle Mode");

  display.setCursor(0, 56);
  display.println("DOWN : Back");

  display.display();
}
void oled_show_mode_menu(int selected) {

  static const char* items[] = {
    "Auto",
    "Manual",
    "Back"
  };

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("MODE");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  for (int i = 0; i < 3; i++) {
    display.setCursor(0, 22 + i * 12);
    display.print(i == selected ? "> " : "  ");
    display.println(items[i]);
  }

  display.display();
}
void oled_show_manual_control(int selected,
                              bool heaterOn,
                              bool coolerOn) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("MANUAL CONTROL");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  // Heater line
  display.setCursor(0, 24);
  display.print(selected == 0 ? "> " : "  ");
  display.print("Heater : ");
  display.println(heaterOn ? "ON" : "OFF");

  // Cooler line
  display.setCursor(0, 36);
  display.print(selected == 1 ? "> " : "  ");
  display.print("Cooler : ");
  display.println(coolerOn ? "ON" : "OFF");

  // Back line
  display.setCursor(0, 48);
  display.print(selected == 2 ? "> " : "  ");
  display.println("Back");

  display.display();
}
void oled_show_hysteresis_menu(int selected,
                               float tempHyst,
                               float humHyst)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Title
  display.setCursor(0, 0);
  display.println("HYSTERESIS SETTINGS");
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  // Temperature Hysteresis
  display.setCursor(0, 24);
  display.print(selected == 0 ? "> " : "  ");
  display.print("Temp Hyst : +-");
  display.print(tempHyst, 1);
  display.print(" C");

  // Humidity Hysteresis
  display.setCursor(0, 36);
  display.print(selected == 1 ? "> " : "  ");
  display.print("Hum  Hyst : +-");
  display.print(humHyst, 0);
  display.print(" %");

  // Back Option
  display.setCursor(0, 48);
  display.print(selected == 2 ? "> " : "  ");
  display.print("Back");

  display.display();
}


