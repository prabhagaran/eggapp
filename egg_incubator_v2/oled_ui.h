#ifndef OLED_UI_H
#define OLED_UI_H

#include "RTClib.h"
#include "globals.h"

// ─────────────────────────────────────────────────────────────────────────────
// INIT
// ─────────────────────────────────────────────────────────────────────────────
bool oled_init(void);  // returns false if OLED hardware not found

// ─────────────────────────────────────────────────────────────────────────────
// HOME SCREENS
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
                               const char* milestoneLabel);

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
                             const char* phaseLabel);

// ─────────────────────────────────────────────────────────────────────────────
// FAULT SCREEN
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_fault_screen(float currentTemp);

// ─────────────────────────────────────────────────────────────────────────────
// MENUS
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_menu(int selected);
void oled_show_egg_incubator_menu(int selected, int topIdx);
void oled_show_climate_chamber_menu(int selected, int topIdx);
void oled_show_system_menu(int selected, int topIdx);
void oled_show_pump_settings(int menuIdx, uint16_t durSec, bool editing);
void oled_show_controller_mode(int selected, ProfileType activeProfile);
void oled_show_set_environment(int selected, ProfileType profile);
void oled_show_settings_menu(int selected);
void oled_show_mode_menu(int selected);
void oled_show_manual_control(int selected, bool heaterOn, bool coolerOn, ProfileType profile);
void oled_show_climate_mode_menu(int selected, ClimateModeType active);

// ─────────────────────────────────────────────────────────────────────────────
// ENVIRONMENT EDIT SCREENS
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_temperature(float current, float setpoint);
void oled_show_humidity(float current, float setpoint);
void oled_show_hysteresis_menu(int selected, float tempHyst, float humHyst);

// ─────────────────────────────────────────────────────────────────────────────
// EGG INCUBATOR SCREENS
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_egg_type(int selected);
void oled_show_incubation_day_set(int selected, int day, int month, int year, bool editing, int editField);
void oled_show_turner_settings(int selected,
                                uint16_t intervalMin,
                                uint16_t durationSec,
                                int editState,
                                bool toggleState);
void oled_show_fan_settings(int selected, uint8_t speedPercent);

// ─────────────────────────────────────────────────────────────────────────────
// CLIMATE CHAMBER SCREENS
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_climate_schedule(int selected,
                                 uint8_t startHour,
                                 uint8_t endHour);
void oled_show_climate_cyclic(int selected,
                               uint32_t heatMin,
                               uint32_t coolMin);
void oled_show_climate_ramp(int selected,
                             uint8_t stepCount,
                             const RampStep_t* steps,
                             uint8_t activeStep);

// ─────────────────────────────────────────────────────────────────────────────
// SETTINGS SCREENS
// ─────────────────────────────────────────────────────────────────────────────
void oled_show_device_info(const char* deviceId,
                            const char* fwVersion,
                            const char* ipAddr,
                            const char* profileName,
                            uint32_t    uptimeSec);

void oled_show_wifi_menu(int selected, bool connected);

void oled_show_factory_reset_confirm(void);

// ─────────────────────────────────────────────────────────────────────────────
// TIME & DATE SCREENS
// ─────────────────────────────────────────────────────────────────────────────
// selected: 0=Manual Set, 1=WiFi Sync, 2=Back
void oled_show_time_date_menu(int selected);
// field: 0=Hour 1=Min 2=Sec 3=Day 4=Month 5=Year 6=SAVE?
void oled_show_time_edit(int field, int h, int m, int s, int d, int mo, int y);
// status: 0=Syncing 1=Time Updated 2=WiFi Not Connected
void oled_show_time_wifi_sync(int status);

#endif // OLED_UI_H
