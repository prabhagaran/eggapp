#ifndef INCUBATOR_LOGIC_H
#define INCUBATOR_LOGIC_H

#include "globals.h"   // EggType, Settings_t, gSettings, mutexes, uint32_t, etc.

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
// Incubator Logic Helpers
// ─────────────────────────────────────────────────────────────

// Apply preset defaults based on egg type
void applyEggTypeDefaults(EggType type);

// Incubation timeline calculations
int      calcIncubationDay(void);
uint32_t calcHatchEpoch(void);
int      calcDaysLeft(void);

// Milestone detection (writes label if milestone hit)
bool checkMilestone(uint32_t nowEpoch, char* outLabel, size_t labelLen);

// Egg turner state logic
bool turnerActive(void);

#ifdef __cplusplus
}
#endif

#endif // INCUBATOR_LOGIC_H
