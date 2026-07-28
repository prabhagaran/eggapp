#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include "config.h"

// One reading set. Each channel carries its own validity bit rather than a
// sentinel value: 0 lux, 0% feed and 0 ppm are all legitimate readings, so
// no in-band sentinel would be safe.
typedef struct {
    float temp_c;
    float humidity_pct;
    float co2_ppm;
    float nh3_ppm;
    float light_lux;
    float feed_pct;
    float water_pct;
    bool  temp_valid;
    bool  hum_valid;
    bool  co2_valid;
    bool  nh3_valid;
    bool  light_valid;
    bool  feed_valid;
    bool  water_valid;
    // True when these values were fabricated by SIMULATE_SENSORS rather
    // than measured. Travels with the payload as "sim":1 so stored history
    // stays distinguishable from real readings — see config.h.
    bool  simulated;
} CoopReadings_t;

void sensorsBegin(void);
// Reads every compiled-in channel. Slow (UART round trip + ultrasonic
// echo windows), so callers must not hold a lock across it.
void sensorsRead(CoopReadings_t* out);

#endif // SENSORS_H
