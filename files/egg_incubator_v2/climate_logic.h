#ifndef CLIMATE_LOGIC_H
#define CLIMATE_LOGIC_H

#include "globals.h"   // uint8_t, uint32_t, RampStep_t, gSettings, mutexes, etc.

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
// Climate Mode Logic Helpers
// ─────────────────────────────────────────────────────────────

// Fixed schedule mode
bool scheduleAllowsHeat(uint8_t currentHour);

// Cyclic mode
bool cyclicInHeatPhase(uint32_t nowEpoch);

// Ramp mode
float getRampTargetTemp(uint32_t nowEpoch);

// Dual actuator temperature control
void applyDualControl(float currentTemp,
                      float targetTemp,
                      float hysteresis,
                      bool  heaterAllowed,
                      bool  coolerAllowed);

#ifdef __cplusplus
}
#endif

#endif // CLIMATE_LOGIC_H