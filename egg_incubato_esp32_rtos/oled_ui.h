#ifndef OLED_UI_H
#define OLED_UI_H

#include "RTClib.h"

void oled_init(void);
void oled_show_home(const DateTime& now,
                    int day,
                    float temp,
                    float hum,
                    float tempSetpoint,
                    float humSetpoint,
                    bool isAutoMode,
                    bool heaterOn,
                    bool coolerOn,
                    bool humidifierOn);

void oled_show_menu(int selected);
void oled_show_controller_mode(int selected);
void oled_show_set_environment(int selected);
void oled_show_temperature(float current, float setpoint);
void oled_show_hysteresis(float hysteresis);
void oled_show_settings_menu(int selected);
void oled_show_heater_control(bool isAutoMode);
void oled_show_mode_menu(int selected);
void oled_show_manual_control(int selected, bool heaterOn, bool coolerOn);
void oled_show_humidity(float current, float setpoint);


#endif
