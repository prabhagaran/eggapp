#include "climate_logic.h"
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>

// ─────────────────────────────────────────────────────────────────────────────
bool scheduleAllowsHeat(uint8_t currentHour) {
    uint8_t startH = DEFAULT_SCHED_START_HOUR;
    uint8_t endH   = DEFAULT_SCHED_END_HOUR;

    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        startH = gSettings.schedStartHour;
        endH   = gSettings.schedEndHour;
        xSemaphoreGive(settingsMutex);
    }

    // Handle midnight-spanning windows (e.g. 22 → 06)
    if (startH <= endH) {
        return (currentHour >= startH && currentHour < endH);
    } else {
        return (currentHour >= startH || currentHour < endH);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
bool cyclicInHeatPhase(uint32_t nowEpoch) {
    uint32_t cycleStart = 0;
    uint32_t heatMin    = DEFAULT_CLIMATE_HEAT_PERIOD_MIN;
    uint32_t coolMin    = DEFAULT_CLIMATE_COOL_PERIOD_MIN;

    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        cycleStart = gSettings.cycleStartEpoch;
        heatMin    = gSettings.heatPeriodMin;
        coolMin    = gSettings.coolPeriodMin;
        xSemaphoreGive(settingsMutex);
    }

    // With an invalid epoch any subtraction underflows; hold heat phase as safe default.
    if (!rtcEpochValid) return true;

    // Bootstrap: if no cycle start recorded, start now in heat phase
    if (cycleStart == 0) {
        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            gSettings.cycleStartEpoch = nowEpoch;
            xSemaphoreGive(settingsMutex);
        }
        // Persist single cycle start key to NVS so power-cycle preserves phase
        {
            Preferences prefs;
            prefs.begin("incubator", false);
            prefs.putULong("cycleStart", nowEpoch);
            prefs.end();
        }
        return true;
    }

    uint32_t totalCycleSec  = (heatMin + coolMin) * 60UL;
    if (totalCycleSec == 0) return true;

    uint32_t elapsed = (nowEpoch - cycleStart) % totalCycleSec;
    return (elapsed < heatMin * 60UL);
}

// ─────────────────────────────────────────────────────────────────────────────
float getRampTargetTemp(uint32_t nowEpoch) {
    uint8_t    stepIdx       = 0;
    uint8_t    stepCount     = 0;
    uint32_t   stepStart     = 0;
    RampStep_t steps[CLIMATE_MAX_RAMP_STEPS];
    float      fallbackTemp  = DEFAULT_TEMP_SETPOINT;

    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        stepIdx     = gSettings.rampStepIdx;
        stepCount   = gSettings.rampCount;
        stepStart   = gSettings.rampStepStartEpoch;
        fallbackTemp= gSettings.tempSetpoint;
        memcpy(steps, gSettings.rampSteps, sizeof(steps));
        xSemaphoreGive(settingsMutex);
    }

    if (stepCount == 0) return fallbackTemp;
    // With an invalid epoch step advancement would fire immediately; hold current setpoint.
    if (!rtcEpochValid) return fallbackTemp;
    if (stepIdx >= stepCount) {
        // Ramp finished — hold last step temperature
        return steps[stepCount - 1].targetTemp;
    }

    // Bootstrap
    if (stepStart == 0) {
        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            gSettings.rampStepStartEpoch = nowEpoch;
            xSemaphoreGive(settingsMutex);
        }
        // Persist ramp state (index + start epoch) so ramp resumes after reboot
        {
            Preferences prefs;
            prefs.begin("incubator", false);
            prefs.putULong("rampStart", nowEpoch);
            prefs.putUInt("rampIdx", (uint32_t)stepIdx);
            prefs.end();
        }
        return steps[stepIdx].targetTemp;
    }

    // Check if the current step has elapsed
    uint32_t stepDurationSec = (uint32_t)steps[stepIdx].durationMin * 60UL;
    if ((nowEpoch - stepStart) >= stepDurationSec) {
        // Advance to next step
        uint8_t nextIdx = stepIdx + 1;
        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            gSettings.rampStepIdx         = nextIdx;
            gSettings.rampStepStartEpoch  = nowEpoch;
            xSemaphoreGive(settingsMutex);
        }
        // Persist updated ramp index/start so progress is durable
        {
            Preferences prefs;
            prefs.begin("incubator", false);
            prefs.putUInt("rampIdx", (uint32_t)nextIdx);
            prefs.putULong("rampStart", nowEpoch);
            prefs.end();
        }
        if (nextIdx >= stepCount) return steps[stepCount - 1].targetTemp;
        return steps[nextIdx].targetTemp;
    }

    return steps[stepIdx].targetTemp;
}

// ─────────────────────────────────────────────────────────────────────────────
// Cooler lockout — prevents compressor short-cycling
static unsigned long coolerLastOffMs = 0;

void applyDualControl(float currentTemp,
                      float targetTemp,
                      float hysteresis,
                      bool  heaterAllowed,
                      bool  coolerAllowed)
{
    bool currentHeater = false;
    bool currentCooler = false;

    if (xSemaphoreTake(controlMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        currentHeater = gRelayState.heaterOn;
        currentCooler = gRelayState.coolerOn;
        xSemaphoreGive(controlMutex);
    }

    bool coolerLocked = ((millis() - coolerLastOffMs) < (COOLER_LOCKOUT_SEC * 1000UL));

    // ── Heater logic ──────────────────────────────────────────────────────
    if (heaterAllowed && !currentCooler) {
        if (!currentHeater && currentTemp <= (targetTemp - hysteresis)) {
            setRelay(RELAY_HEATER, true);
            Serial.printf("[CLIMATE] Heater ON (%.1f / %.1f)\n", currentTemp, targetTemp);
        } else if (currentHeater && currentTemp >= targetTemp) {
            setRelay(RELAY_HEATER, false);
            Serial.printf("[CLIMATE] Heater OFF (%.1f / %.1f)\n", currentTemp, targetTemp);
        }
    } else if (!heaterAllowed && currentHeater) {
        setRelay(RELAY_HEATER, false);
    }

    // ── Cooler logic ──────────────────────────────────────────────────────
    if (coolerAllowed && !currentHeater && !coolerLocked) {
        if (!currentCooler && currentTemp >= (targetTemp + hysteresis)) {
            setRelay(RELAY_COOLER, true);
            Serial.printf("[CLIMATE] Cooler ON (%.1f / %.1f)\n", currentTemp, targetTemp);
        } else if (currentCooler && currentTemp <= targetTemp) {
            setRelay(RELAY_COOLER, false);
            coolerLastOffMs = millis();
            Serial.printf("[CLIMATE] Cooler OFF (%.1f / %.1f)\n", currentTemp, targetTemp);
        }
    } else if (!coolerAllowed && currentCooler) {
        setRelay(RELAY_COOLER, false);
        coolerLastOffMs = millis();
    }
}
