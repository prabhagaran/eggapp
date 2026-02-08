#ifndef OLED_UI_H
#define OLED_UI_H

#include "RTClib.h"

void oled_init(void);
void oled_show_home(const DateTime& now, int day, float temp, float hum);
void oled_show_menu(int selected);
void oled_show_controller_mode(int selected);
void oled_show_set_environment(int selected);
void oled_show_temperature(float current, float setpoint);
void oled_show_hysteresis(float hysteresis);



#endif
