#include "oled_ui.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static const char* MONTH_NAMES[] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
};

// ─────────────────────────────────────────────────────────────────────────────
void oled_init(void) {
    // Wire is already begun in setup() — do not call Wire.begin() again
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        while (1);  // halt on OLED failure
    }
    display.clearDisplay();
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// HOME — Egg Incubator
// Layout (128×64):
//   0-7   : AUTO/MAN  |  HH:MM
//   8-15  : Day X/Y   |  DD Mon
//   16    : divider
//   17-25 : Temp XX.X / XX.X C
//   26-34 : Hum  XX / XX %
//   35-43 : Hatch in X days  (or milestone banner)
//   44-52 : Heat:ON  Fan:ON
//   53-63 : Humid:ON  Turn:ON  (NO WIFI if offline)
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_home_incubator(const DateTime& now,
                               int     day,
                               int     daysLeft,
                               float   temp,
                               float   hum,
                               float   tempSP,
                               float   humSP,
                               bool    isAuto,
                               bool    heaterOn,
                               bool    humidifierOn,
                               bool    fanOn,
                               bool    turnerOn,
                               bool    wifiOk,
                               const char* milestoneLabel)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Row 0: mode | time
    display.setCursor(0, 0);
    display.print(isAuto ? "AUTO" : "MAN");

    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", now.hour(), now.minute());
    display.setCursor(88, 0);
    display.print(timeBuf);

    // Row 1: incubation day | date
    display.setCursor(0, 8);
    if (day > 0) {
        display.print("Day ");
        display.print(day);
    } else {
        display.print("Not started");
    }

    char dateBuf[9];
    snprintf(dateBuf, sizeof(dateBuf), "%02d %s", now.day(), MONTH_NAMES[now.month() - 1]);
    display.setCursor(88, 8);
    display.print(dateBuf);

    // Divider
    display.drawLine(0, 16, 127, 16, SSD1306_WHITE);

    // Temperature
    display.setCursor(0, 18);
    display.print("T:");
    display.print(temp, 1);
    display.print("/");
    display.print(tempSP, 1);
    display.print("C");

    // Humidity
    display.setCursor(0, 27);
    display.print("H:");
    display.print((int)hum);
    display.print("/");
    display.print((int)humSP);
    display.print("%");

    // Hatch countdown or milestone banner
    display.setCursor(0, 36);
    if (milestoneLabel && milestoneLabel[0] != '\0') {
        display.print(">> ");
        display.print(milestoneLabel);
    } else if (daysLeft > 0) {
        display.print("Hatch in ");
        display.print(daysLeft);
        display.print("d");
    } else if (daysLeft == 0) {
        display.print("HATCH DAY!");
    } else {
        display.print("No start date");
    }

    // Relay states row 1
    display.setCursor(0, 46);
    display.print("Ht:");
    display.print(heaterOn ? "ON " : "OFF");
    display.setCursor(64, 46);
    display.print("Fan:");
    display.print(fanOn ? "ON" : "OFF");

    // Relay states row 2 + WiFi
    display.setCursor(0, 55);
    display.print("Hm:");
    display.print(humidifierOn ? "ON " : "OFF");
    display.setCursor(64, 55);
    display.print("Tr:");
    display.print(turnerOn ? "ON " : "OFF");
    if (!wifiOk) {
        display.setCursor(96, 55);
        display.print("!W");
    }

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// HOME — Climate Chamber
// Layout:
//   0-7   : AUTO/MAN  | HH:MM
//   8-15  : CLIMATE   | DD Mon
//   16    : divider
//   17-25 : T: XX.X / XX.X C
//   26-34 : H: XX / XX %
//   35-43 : Phase: HEAT / COOL / SCHED
//   44-52 : Heat:ON  Cool:ON
//   53-63 : Humid:ON         !W (if offline)
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_home_climate(const DateTime& now,
                             float  temp,
                             float  hum,
                             float  tempSP,
                             float  humSP,
                             bool   isAuto,
                             bool   heaterOn,
                             bool   coolerOn,
                             bool   humidifierOn,
                             bool   wifiOk,
                             const char* phaseLabel)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print(isAuto ? "AUTO" : "MAN");

    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", now.hour(), now.minute());
    display.setCursor(88, 0);
    display.print(timeBuf);

    display.setCursor(0, 8);
    display.print("CLIMATE");

    char dateBuf[9];
    snprintf(dateBuf, sizeof(dateBuf), "%02d %s", now.day(), MONTH_NAMES[now.month() - 1]);
    display.setCursor(88, 8);
    display.print(dateBuf);

    display.drawLine(0, 16, 127, 16, SSD1306_WHITE);

    display.setCursor(0, 18);
    display.print("T:");
    display.print(temp, 1);
    display.print("/");
    display.print(tempSP, 1);
    display.print("C");

    display.setCursor(0, 27);
    display.print("H:");
    display.print((int)hum);
    display.print("/");
    display.print((int)humSP);
    display.print("%");

    display.setCursor(0, 36);
    display.print("Phase:");
    if (phaseLabel && phaseLabel[0] != '\0') {
        display.print(phaseLabel);
    } else {
        display.print("--");
    }

    display.setCursor(0, 46);
    display.print("Heat:");
    display.print(heaterOn ? "ON " : "OFF");
    display.setCursor(64, 46);
    display.print("Cool:");
    display.print(coolerOn ? "ON" : "OFF");

    display.setCursor(0, 55);
    display.print("Humid:");
    display.print(humidifierOn ? "ON " : "OFF");
    if (!wifiOk) {
        display.setCursor(96, 55);
        display.print("!W");
    }

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// FAULT SCREEN
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_fault_screen(float currentTemp) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(10, 2);
    display.print("! ALARM !");

    display.drawLine(0, 20, 127, 20, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 26);
    display.print("Over Temperature");

    display.setCursor(0, 37);
    display.print("Temp: ");
    display.print(currentTemp, 1);
    display.print(" C");

    display.setCursor(0, 50);
    display.print("Hold OK 3s to Reset");

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// MAIN MENU
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_menu(int selected) {
    static const char* items[] = {
        "Controller Mode",
        "Set Environment",
        "Settings",
        "Exit"
    };
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("MAIN MENU");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    for (int i = 0; i < 4; i++) {
        display.setCursor(0, 16 + i * 12);
        display.print(i == selected ? "> " : "  ");
        display.print(items[i]);
    }
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// CONTROLLER MODE MENU
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_controller_mode(int selected, ProfileType activeProfile) {
    static const char* items[] = {
        "Egg Incubator",
        "Climate Chamber",
        "Back"
    };
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("CONTROLLER MODE");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Show active profile indicator
    display.setCursor(0, 16);
    display.print("Active: ");
    display.print(activeProfile == PROFILE_EGG_INCUBATOR ? "Incubator" : "Climate");

    for (int i = 0; i < 3; i++) {
        display.setCursor(0, 26 + i * 12);
        display.print(i == selected ? "> " : "  ");
        display.print(items[i]);
    }
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// SET ENVIRONMENT MENU — profile-aware (hides irrelevant items)
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_set_environment(int selected, ProfileType profile) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("SET ENVIRONMENT");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Build full menu list so we can paginate a window around the selected item
    const char* fullItems[12];
    int fullCount = 0;

    // Common items
    fullItems[fullCount++] = "Temperature";
    fullItems[fullCount++] = "Hysteresis";
    fullItems[fullCount++] = "Humidity";

    if (profile == PROFILE_EGG_INCUBATOR) {
        fullItems[fullCount++] = "Inc. Start Day";
        fullItems[fullCount++] = "Egg Type";
        fullItems[fullCount++] = "Turner";
        fullItems[fullCount++] = "Fan";
        fullItems[fullCount++] = "Back";
    } else {
        fullItems[fullCount++] = "Climate Profile";
        fullItems[fullCount++] = "Back";
    }

    // Determine visible window size (max rows that fit)
    int maxRows = 5; // rows at y=14,24,34,44,54
    if (fullCount < maxRows) maxRows = fullCount;

    // Compute window start so selected stays visible and ideally centered
    int start = selected - (maxRows / 2);
    if (start < 0) start = 0;
    if (start > fullCount - maxRows) start = max(0, fullCount - maxRows);

    // Render window
    for (int r = 0; r < maxRows; r++) {
        int idx = start + r;
        display.setCursor(0, 16 + r * 10);
        display.print(idx == selected ? "> " : "  ");
        display.print(fullItems[idx]);
    }

    // If there are more pages, show a small pager indicator on the right
    if (fullCount > maxRows) {
        int page = (start / maxRows) + 1;
        int totalPages = (fullCount + maxRows - 1) / maxRows;
        char pbuf[6];
        snprintf(pbuf, sizeof(pbuf), "%d/%d", page, totalPages);
        display.setCursor(96, 54);
        display.print(pbuf);
    }

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// TEMPERATURE EDIT
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_temperature(float current, float setpoint) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("TEMPERATURE");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 18);
    display.print("Current : ");
    display.print(current, 1);
    display.print(" C");

    display.setCursor(0, 32);
    display.print("Setpoint: ");
    display.print(setpoint, 1);
    display.print(" C");

    display.setCursor(0, 50);
    display.setTextSize(1);
    display.print("UP/DN adjust  OK save");

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// HUMIDITY EDIT
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_humidity(float current, float setpoint) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("HUMIDITY");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 18);
    display.print("Current : ");
    display.print((int)current);
    display.print(" %");

    display.setCursor(0, 32);
    display.print("Setpoint: ");
    display.print((int)setpoint);
    display.print(" %");

    display.setCursor(0, 50);
    display.print("UP/DN adjust  OK save");

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// HYSTERESIS MENU
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_hysteresis_menu(int selected, float tempHyst, float humHyst) {
    static const char* items[] = { "Temp Hyst", "Hum  Hyst", "Back" };
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("HYSTERESIS");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 14);
    display.print(selected == 0 ? "> " : "  ");
    display.print("Temp: +-");
    display.print(tempHyst, 1);
    display.print(" C");

    display.setCursor(0, 28);
    display.print(selected == 1 ? "> " : "  ");
    display.print("Hum : +-");
    display.print(humHyst, 0);
    display.print(" %");

    display.setCursor(0, 42);
    display.print(selected == 2 ? "> " : "  ");
    display.print("Back");

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// EGG TYPE SELECTION
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_egg_type(int selected) {
    static const char* items[] = {
        "Chicken  21d 37.5C",
        "Duck     28d 37.5C",
        "Quail    17d 37.5C",
        "Custom",
        "Back"
    };
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("EGG TYPE");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    for (int i = 0; i < 5; i++) {
        display.setCursor(0, 14 + i * 10);
        display.print(i == selected ? "> " : "  ");
        display.print(items[i]);
    }
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// INCUBATION START DATE SET
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_incubation_day_set(int selected, int day, int month, int year, bool editing, int editField) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("INCUBATION");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Determine display date: prefer passed (edit) values when editing,
    // otherwise prefer saved startEpoch, and fall back to RTC date.
    int d = day, m = month, y = year;
    if (!editing) {
        uint32_t startEpoch = 0;
        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
            startEpoch = gSettings.startEpoch;
            xSemaphoreGive(settingsMutex);
        }
        if (startEpoch != 0) {
            DateTime sd((uint32_t)startEpoch);
            d = sd.day(); m = sd.month(); y = sd.year();
        } else {
            DateTime now(2024,1,1);
            if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                now = gRtcTime.now;
                xSemaphoreGive(rtcMutex);
            }
            d = now.day(); m = now.month(); y = now.year();
        }
    }

    // Row 1: Start Date
    display.setCursor(0, 18);
    display.print(selected == 0 ? "> " : "  ");
    display.print("Date : ");
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d", d, m, y);
    display.print(buf);

    // Row 2: Back
    display.setCursor(0, 34);
    display.print(selected == 1 ? "> " : "  ");
    display.print("Back");

    // Footer: editing hints when in edit mode
    display.setTextSize(1);
    display.setCursor(0, 54);
    if (editing) {
        const char* fieldName = (editField == 0) ? "Day" : (editField == 1) ? "Month" : "Year";
        char hint[32];
        snprintf(hint, sizeof(hint), "Edit: %s  UP/DN change", fieldName);
        display.print(hint);
    } 

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// TURNER SETTINGS
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_turner_settings(int selected,
                                uint16_t intervalMin,
                                uint16_t durationSec,
                                int editState,
                                bool toggleState)
{
    // editState: 0=nav,1=edit-interval,2=edit-duration,3=toggle
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("TURNER SETTINGS");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Interval row
    display.setCursor(0, 14);
    display.print(selected == 0 ? "> " : "  ");
    display.print("Interval: ");
    display.print(intervalMin);
    display.print(" min");
    // edit indicator removed per user request

    // Duration row
    display.setCursor(0, 26);
    display.print(selected == 1 ? "> " : "  ");
    display.print("Duration: ");
    display.print(durationSec);
    display.print(" sec");
    // edit indicator removed per user request

    // Turn Now row — show toggle state when in toggle mode
    display.setCursor(0, 38);
    display.print(selected == 2 ? "> " : "  ");
    display.print("Turn Now: ");
    display.print(toggleState ? "ON" : "OFF");
    // toggle indicator removed per user request

    // Back row
    display.setCursor(0, 50);
    display.print(selected == 3 ? "> " : "  ");
    display.print("Back");

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// FAN SETTINGS
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_fan_settings(int selected, uint8_t speedPercent)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("FAN SETTINGS");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Speed row (0 = Speed, 1 = Back)
    display.setCursor(0, 14);
    display.print(selected == 0 ? "> " : "  ");
    display.print("Speed: ");
    display.print((int)speedPercent);
    display.print(" %");

    display.setCursor(0, 34);
    display.print(selected == 1 ? "> " : "  ");
    display.print("Back");

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// SETTINGS MENU
// ─────────────────────────────────────────────────────────────────────────────
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
    display.print("SETTINGS");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Build full menu list so we can paginate a window around the selected item
    const char* fullItems[6];
    int fullCount = 0;
    for (int i = 0; i < 6; i++) fullItems[fullCount++] = items[i];

    // Determine visible window size (max rows that fit)
    int maxRows = 5; // rows at y=16,26,36,46,56 (fits on 64px OLED)
    if (fullCount < maxRows) maxRows = fullCount;

    // Compute window start so selected stays visible and ideally centered
    int start = selected - (maxRows / 2);
    if (start < 0) start = 0;
    if (start > fullCount - maxRows) start = max(0, fullCount - maxRows);

    // Render only visible rows
    for (int r = 0; r < maxRows; r++) {
        int idx = start + r;
        display.setCursor(0, 16 + r * 10);
        display.print(idx == selected ? "> " : "  ");
        display.print(fullItems[idx]);
    }

    // Pager indicator if needed
    if (fullCount > maxRows) {
        int page = (start / maxRows) + 1;
        int totalPages = (fullCount + maxRows - 1) / maxRows;
        char pbuf[6];
        snprintf(pbuf, sizeof(pbuf), "%d/%d", page, totalPages);
        display.setCursor(96, 54);
        display.print(pbuf);
    }

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// MODE MENU (Auto / Manual / Back)
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_mode_menu(int selected) {
    static const char* items[] = { "Auto", "Manual", "Back" };
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("CONTROL MODE");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    for (int i = 0; i < 3; i++) {
        display.setCursor(0, 18 + i * 14);
        display.print(i == selected ? "> " : "  ");
        display.print(items[i]);
    }
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// MANUAL CONTROL — profile-aware (hides cooler in incubator mode)
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_manual_control(int selected, bool heaterOn, bool coolerOn, ProfileType profile) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("MANUAL CONTROL");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 16);
    display.print(selected == 0 ? "> " : "  ");
    display.print("Heater : ");
    display.print(heaterOn ? "ON" : "OFF");

    if (profile == PROFILE_CLIMATE_CHAMBER) {
        display.setCursor(0, 28);
        display.print(selected == 1 ? "> " : "  ");
        display.print("Cooler : ");
        display.print(coolerOn ? "ON" : "OFF");

        display.setCursor(0, 40);
        display.print(selected == 2 ? "> " : "  ");
        display.print("Back");
    } else {
        display.setCursor(0, 28);
        display.print(selected == 1 ? "> " : "  ");
        display.print("Back");
    }

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// CLIMATE MODE MENU
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_climate_mode_menu(int selected, ClimateModeType active) {
    static const char* items[] = { "Fixed Schedule", "Cyclic Period", "Ramp Profile", "Back" };
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("CLIMATE PROFILE");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 12);
    display.print("Active: ");
    switch (active) {
        case CLIMATE_FIXED_SCHEDULE: display.print("Fixed"); break;
        case CLIMATE_CYCLIC:         display.print("Cyclic"); break;
        case CLIMATE_RAMP:           display.print("Ramp"); break;
    }

    for (int i = 0; i < 4; i++) {
        display.setCursor(0, 26 + i * 10);
        display.print(i == selected ? "> " : "  ");
        display.print(items[i]);
    }
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// CLIMATE FIXED SCHEDULE EDIT
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_climate_schedule(int selected, uint8_t startHour, uint8_t endHour) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("FIXED SCHEDULE");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 16);
    display.print("Heat from hour:");

    display.setCursor(0, 28);
    display.print(selected == 0 ? "> " : "  ");
    display.print("Start: ");
    display.print(startHour);
    display.print(":00");

    display.setCursor(0, 40);
    display.print(selected == 1 ? "> " : "  ");
    display.print("End  : ");
    display.print(endHour);
    display.print(":00");

    display.setCursor(0, 52);
    display.print(selected == 2 ? "> " : "  ");
    display.print("Back");

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// CLIMATE CYCLIC EDIT
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_climate_cyclic(int selected, uint32_t heatMin, uint32_t coolMin) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("CYCLIC PERIOD");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 16);
    display.print(selected == 0 ? "> " : "  ");
    display.print("Heat: ");
    display.print(heatMin);
    display.print(" min");

    display.setCursor(0, 28);
    display.print(selected == 1 ? "> " : "  ");
    display.print("Cool: ");
    display.print(coolMin);
    display.print(" min");

    uint32_t totalH = (heatMin + coolMin) / 60;
    uint32_t totalM = (heatMin + coolMin) % 60;
    display.setCursor(0, 40);
    display.print("Total: ");
    display.print(totalH);
    display.print("h ");
    display.print(totalM);
    display.print("m");

    display.setCursor(0, 52);
    display.print(selected == 2 ? "> " : "  ");
    display.print("Back");

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// CLIMATE RAMP PROFILE VIEW/EDIT
// Shows up to 3 steps at a time with scrolling via selected index.
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_climate_ramp(int selected,
                             uint8_t stepCount,
                             const RampStep_t* steps,
                             uint8_t activeStep)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("RAMP PROFILE");
    char buf[14];
    snprintf(buf, sizeof(buf), " Step %d/%d", activeStep + 1, stepCount);
    display.print(buf);
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (stepCount == 0) {
        display.setCursor(0, 24);
        display.print("No steps configured.");
        display.setCursor(0, 36);
        display.print("Add via Set Env.");
        display.display();
        return;
    }

    // Show up to 4 steps, scroll window around selected
    int startIdx = max(0, selected - 1);
    int shown    = 0;
    for (int i = startIdx; i < (int)stepCount && shown < 4; i++, shown++) {
        display.setCursor(0, 14 + shown * 12);
        display.print(i == selected ? ">" : " ");
        display.print(i + 1);
        display.print(":");
        display.print(steps[i].durationMin);
        display.print("m@");
        display.print(steps[i].targetTemp, 1);
        display.print("C");
        if (i == (int)activeStep) {
            display.print("*");  // marks currently running step
        }
    }

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// DEVICE INFO SCREEN
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_device_info(const char* deviceId,
                            const char* fwVersion,
                            const char* ipAddr,
                            const char* profileName,
                            uint32_t    uptimeSec)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("DEVICE INFO");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 14);
    display.print("ID: ");
    display.print(deviceId);

    display.setCursor(0, 24);
    display.print("FW: ");
    display.print(fwVersion);

    display.setCursor(0, 34);
    display.print("IP: ");
    display.print(ipAddr);

    display.setCursor(0, 44);
    display.print("Profile: ");
    display.print(profileName);

    uint32_t hours   = uptimeSec / 3600;
    uint32_t minutes = (uptimeSec % 3600) / 60;
    display.setCursor(0, 54);
    display.print("Up: ");
    display.print(hours);
    display.print("h");
    display.print(minutes);
    display.print("m");

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// FACTORY RESET CONFIRM
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_factory_reset_confirm(void) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("!! FACTORY RESET !!");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 18);
    display.print("All settings will be");
    display.setCursor(0, 28);
    display.print("erased and device");
    display.setCursor(0, 38);
    display.print("will restart.");
    display.setCursor(0, 50);
    display.print("OK=Confirm  DN=Cancel");

    display.display();
}
