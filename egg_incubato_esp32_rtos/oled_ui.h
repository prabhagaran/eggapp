#ifndef OLED_UI_H
#define OLED_UI_H

#include "RTClib.h"

void oled_init(void);
void oled_show_home(const DateTime& now, int day, float temp, float hum);
void oled_show_menu(int selected);

#endif
