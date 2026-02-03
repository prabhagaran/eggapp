#ifndef OLED_UI_H
#define OLED_UI_H

void oled_init(void);
void oled_show_boot(void);
void oled_show_status(float temp, float hum, int day, bool heater, bool wifi);

#endif
