#include "oled_ui.h"

void setup()
{
  oled_init();
  oled_show_boot();
  delay(2000);
}

void loop()
{
  // Dummy values for now
  oled_show_status(
    37.5,   // temperature
    55.0,   // humidity
    3,      // day
    true,   // heater ON
    true    // wifi OK
  );

  delay(1000);
}
