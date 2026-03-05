#include "oled_ui.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ─────────────────────────────────────────────────────────────────────────────
// SHARED MENU PRIMITIVES
// Layout (128×64): title y=0..9 | divider y=10 | 4 item rows y=12..63 (13 px each)
// Scrollbar: 3 px wide at x=125
// ─────────────────────────────────────────────────────────────────────────────
static const int MENU_Y0   = 16;   // y of first highlight bar (text at y+2=18)
static const int ITEM_H    = 13;   // row height (12 px content + 1 px gap)
static const int MENU_ROWS = 4;    // visible rows
static const int SB_X      = 125;  // scrollbar left edge (3 px wide: 125-127)
static const int SB_Y0     = 16;
static const int SB_Y1     = 63;

static void drawMenuTitle(const char* title) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print(title);
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
}

// Render up to MENU_ROWS items with highlight bar + optional scrollbar.
// items/total : full array.  selected : absolute highlighted index.
// topIdx      : first visible index.
static void drawMenuItems(const char** items, int total, int selected, int topIdx) {
    bool sb    = (total > MENU_ROWS);
    int  textW = sb ? (SB_X - 2) : 123;

    for (int row = 0; row < MENU_ROWS; row++) {
        int idx = topIdx + row;
        if (idx >= total) break;
        int y = MENU_Y0 + row * ITEM_H;
        if (idx == selected) {
            display.fillRect(0, y, textW + 1, ITEM_H - 1, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(2, y + 2);
        display.print(items[idx]);
    }
    display.setTextColor(SSD1306_WHITE);

    if (sb) {
        int trackH = SB_Y1 - SB_Y0 + 1;
        int thumbH = max(4, trackH * MENU_ROWS / total);
        int thumbY = (total > 1)
                   ? SB_Y0 + (trackH - thumbH) * selected / (total - 1)
                   : SB_Y0;
        display.drawRect(SB_X, SB_Y0, 3, trackH, SSD1306_WHITE);
        display.fillRect(SB_X, thumbY, 3, thumbH, SSD1306_WHITE);
    }
}

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
        "Egg Incubator", "Climate Chamber", "System", "Back"
    };
    display.clearDisplay();
    drawMenuTitle("MAIN MENU");
    drawMenuItems(items, 4, selected, 0);
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// CONTROLLER MODE MENU
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_controller_mode(int selected, ProfileType activeProfile) {
    static const char* items[] = { "Egg Incubator", "Climate Chamber", "Back" };
    display.clearDisplay();
    drawMenuTitle("CONTROLLER MODE");
    // Info row (non-selectable)
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(2, MENU_Y0 + 2);
    display.print("Active: ");
    display.print(activeProfile == PROFILE_EGG_INCUBATOR ? "Incubator" : "Climate");
    // Selectable rows offset one row below the info row
    for (int row = 0; row < 3; row++) {
        int y = MENU_Y0 + ITEM_H * (row + 1);
        if (row == selected) {
            display.fillRect(0, y, 123, ITEM_H - 1, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(2, y + 2);
        display.print(items[row]);
    }
    display.setTextColor(SSD1306_WHITE);
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
    display.clearDisplay();
    drawMenuTitle("HYSTERESIS");
    char t[22], h[22];
    snprintf(t, sizeof(t), "Temp: +-%.1f C", tempHyst);
    snprintf(h, sizeof(h), "Hum : +-%.0f %%", humHyst);
    const char* items[] = { t, h, "Back" };
    drawMenuItems(items, 3, selected, 0);
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
    const int total = 5;
    int top = selected - MENU_ROWS / 2;
    if (top < 0) top = 0;
    if (top > total - MENU_ROWS) top = total - MENU_ROWS;
    if (top < 0) top = 0;
    display.clearDisplay();
    drawMenuTitle("EGG TYPE");
    drawMenuItems(items, total, selected, top);
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
    display.clearDisplay();
    drawMenuTitle("TURNER SETTINGS");
    char intv[22], dur[22], tnow[16];
    snprintf(intv, sizeof(intv), "Interval: %d min",  (int)intervalMin);
    snprintf(dur,  sizeof(dur),  "Duration: %d sec",  (int)durationSec);
    snprintf(tnow, sizeof(tnow), "Turn Now: %s",      toggleState ? "ON" : "OFF");
    const char* items[] = { intv, dur, tnow, "Back" };
    drawMenuItems(items, 4, selected, 0);
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// FAN SETTINGS
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_fan_settings(int selected, uint8_t speedPercent)
{
    display.clearDisplay();
    drawMenuTitle("FAN SETTINGS");
    char spd[18];
    snprintf(spd, sizeof(spd), "Speed: %d %%", (int)speedPercent);
    const char* items[] = { spd, "Back" };
    drawMenuItems(items, 2, selected, 0);
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// SETTINGS MENU
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_settings_menu(int selected) {
    static const char* items[] = {
        "Time & Date", "WiFi", "Heater Control",
        "Device Info", "Factory Reset", "Back"
    };
    const int total = 6;
    int top = selected - MENU_ROWS / 2;
    if (top < 0) top = 0;
    if (top > total - MENU_ROWS) top = total - MENU_ROWS;
    display.clearDisplay();
    drawMenuTitle("SETTINGS");
    drawMenuItems(items, total, selected, top);
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// MODE MENU (Auto / Manual / Back)
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_mode_menu(int selected) {
    static const char* items[] = { "Auto", "Manual", "Back" };
    display.clearDisplay();
    drawMenuTitle("CONTROL MODE");
    drawMenuItems(items, 3, selected, 0);
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
    static const char* items[] = {
        "Fixed Schedule", "Cyclic Period", "Ramp Profile", "Back"
    };
    display.clearDisplay();
    // Show active mode in title area
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("CLIMATE PROFILE");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    drawMenuItems(items, 4, selected, 0);
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
        display.setCursor(0, 16 + shown * 12);
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

    display.setCursor(0, 16);
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

// ─────────────────────────────────────────────────────────────────────────────
// WIFI MENU
// selected: 0 = Connect/Disconnect,  1 = Back
// connected: current WiFi.status() == WL_CONNECTED
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_wifi_menu(int selected, bool connected) {
    display.clearDisplay();
    drawMenuTitle("WiFi");
    // Status info row (non-selectable)
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(2, MENU_Y0 + 2);
    display.print("Status: ");
    display.print(connected ? "Connected" : "Disconnected");
    // Selectable rows start one row below the info row
    const char* act = connected ? "Disconnect" : "Connect";
    const char* rows[] = { act, "Back" };
    for (int row = 0; row < 2; row++) {
        int y = MENU_Y0 + ITEM_H * (row + 1);
        if (row == selected) {
            display.fillRect(0, y, 123, ITEM_H - 1, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(2, y + 2);
        display.print(rows[row]);
    }
    display.setTextColor(SSD1306_WHITE);
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// TIME & DATE MENU  (Settings → Time & Date)
// selected: 0=Manual Set  1=WiFi Sync  2=Back
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_time_date_menu(int selected) {
    static const char* items[] = { "Manual Set", "WiFi Sync", "Back" };
    display.clearDisplay();
    drawMenuTitle("TIME & DATE");
    drawMenuItems(items, 3, selected, 0);
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// TIME MANUAL EDIT
// field: 0=Hour 1=Min 2=Sec 3=Day 4=Month 5=Year 6=SAVE?
// Shows all 6 editable fields; highlights the active one.
// field==6 shows a save-confirmation prompt.
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_time_edit(int field, int h, int m, int s, int d, int mo, int y) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    if (field == 6) {
        // Save confirmation screen
        display.setCursor(0, 0);
        display.print("SET TIME & DATE");
        display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
        display.setCursor(0, 16);
        display.print("Save changes?");
        display.setCursor(0, 32);
        display.printf("  %02d:%02d:%02d", h, m, s);
        display.setCursor(0, 42);
        display.printf("  %02d/%02d/%04d", d, mo, y);
        display.setCursor(0, 54);
        display.print("OK=Save  DN=Cancel");
        display.display();
        return;
    }

    display.setCursor(0, 0);
    display.print("SET TIME & DATE");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Labels and values for each field
    const char* labels[] = { "Hour  ", "Minute", "Second", "Day   ", "Month ", "Year  " };
    int values[]          = {  h,        m,        s,        d,        mo,       y        };

    for (int i = 0; i < 6; i++) {
        int y_pos = 12 + i * 9;
        display.setCursor(0, y_pos);
        if (i == field) {
            display.print(">");
        } else {
            display.print(" ");
        }
        display.print(labels[i]);
        display.print(":");
        if (i == 5) {
            display.printf(" %04d", values[i]);
        } else {
            display.printf(" %02d", values[i]);
        }
        if (i == field) {
            display.print(" <");
        }
    }

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// EGG INCUBATOR MENU — 10-item list, 4 visible at a time, scrollable
// Items: Control Mode / Set Temperature / Set Humidity / Hysteresis /
//        Egg Type / Incubation Day / Turner / Fan / Pump Duration / Back
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_egg_incubator_menu(int selected, int topIdx) {
    static const char* items[] = {
        "Control Mode",  "Set Temperature", "Set Humidity", "Hysteresis",
        "Egg Type",      "Incubation Day",  "Turner",       "Fan",
        "Pump Duration", "Back"
    };
    display.clearDisplay();
    drawMenuTitle("EGG INCUBATOR");
    drawMenuItems(items, 10, selected, topIdx);
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// CLIMATE CHAMBER MENU
// Items: Control Mode / Set Temperature / Set Humidity / Hysteresis /
//        Climate Mode / Back
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_climate_chamber_menu(int selected, int topIdx) {
    static const char* items[] = {
        "Control Mode",  "Set Temperature", "Set Humidity",
        "Hysteresis",    "Climate Mode",    "Back"
    };
    display.clearDisplay();
    drawMenuTitle("CLIMATE CHAMBER");
    drawMenuItems(items, 6, selected, topIdx);
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// SYSTEM MENU
// Items: Switch Profile / WiFi / Time & Date / Device Info / Factory Reset / Back
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_system_menu(int selected, int topIdx) {
    static const char* items[] = {
        "Switch Profile", "WiFi",           "Time & Date",
        "Device Info",    "Factory Reset",  "Back"
    };
    display.clearDisplay();
    drawMenuTitle("SYSTEM");
    drawMenuItems(items, 6, selected, topIdx);
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// PUMP SETTINGS
// menuIdx 0 = Duration row, 1 = Back row
// editing = true -> show value in brackets, UP/DOWN adjust
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_pump_settings(int menuIdx, uint16_t durSec, bool editing) {
    display.clearDisplay();
    drawMenuTitle("PUMP DURATION");
    char dur[24];
    if (editing)
        snprintf(dur, sizeof(dur), "Duration: [%ds]", (int)durSec);
    else
        snprintf(dur, sizeof(dur), "Duration: %ds",   (int)durSec);
    const char* items[] = { dur, "Back" };
    drawMenuItems(items, 2, menuIdx, 0);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 56);
    display.print(editing ? "UP/DN=+/-5s  OK=Save" : "OK=Edit");
    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// TIME WIFI SYNC SCREEN
// status: 0=Syncing  1=Time Updated  2=WiFi Not Connected
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_time_wifi_sync(int status) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("WiFi Time Sync");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 24);
    switch (status) {
        case 0: display.print("Syncing Time...");      break;
        case 1: display.print("Time Updated!");        break;
        case 2: display.print("WiFi Not Connected");   break;
        default: display.print("Sync Failed");         break;
    }

    display.display();
}
