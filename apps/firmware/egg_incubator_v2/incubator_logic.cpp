#include "incubator_logic.h"
#include "config.h"
#include <Arduino.h>

void applyEggTypeDefaults(EggType type) {
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(200)) != pdTRUE) return;

    gSettings.eggType = type;

    switch (type) {
        case EGG_CHICKEN:
            gSettings.totalDays      = CHICKEN_TOTAL_DAYS;
            gSettings.lockdownDay    = CHICKEN_LOCKDOWN_DAY;
            gSettings.tempSetpoint   = CHICKEN_DEFAULT_TEMP;
            gSettings.humSetpoint    = CHICKEN_DEFAULT_HUM;
            break;
        case EGG_DUCK:
            gSettings.totalDays      = DUCK_TOTAL_DAYS;
            gSettings.lockdownDay    = DUCK_LOCKDOWN_DAY;
            gSettings.tempSetpoint   = DUCK_DEFAULT_TEMP;
            gSettings.humSetpoint    = DUCK_DEFAULT_HUM;
            break;
        case EGG_QUAIL:
            gSettings.totalDays      = QUAIL_TOTAL_DAYS;
            gSettings.lockdownDay    = QUAIL_LOCKDOWN_DAY;
            gSettings.tempSetpoint   = QUAIL_DEFAULT_TEMP;
            gSettings.humSetpoint    = QUAIL_DEFAULT_HUM;
            break;
        case EGG_CUSTOM:
            // Custom: leave existing values, user edits manually
            break;
        default:
            break;
    }

    xSemaphoreGive(settingsMutex);
}

int calcIncubationDay(void) {
    uint32_t startEpoch = 0;
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        startEpoch = gSettings.startEpoch;
        xSemaphoreGive(settingsMutex);
    }
    if (startEpoch == 0) return 0;

    uint32_t nowEpoch = 0;
    if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        nowEpoch = gRtcTime.epoch;
        xSemaphoreGive(rtcMutex);
    }
    if (!rtcEpochValid) return 0;
    if (nowEpoch < startEpoch) return 1;

    return (int)((nowEpoch - startEpoch) / 86400UL) + 1;
}

uint32_t calcHatchEpoch(void) {
    uint32_t startEpoch = 0;
    uint16_t totalDays  = 0;
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        startEpoch = gSettings.startEpoch;
        totalDays  = gSettings.totalDays;
        xSemaphoreGive(settingsMutex);
    }
    if (startEpoch == 0 || totalDays == 0) return 0;
    return startEpoch + ((uint32_t)totalDays * 86400UL);
}

int calcDaysLeft(void) {
    uint32_t hatchEpoch = calcHatchEpoch();
    if (hatchEpoch == 0) return -1;

    uint32_t nowEpoch = 0;
    if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        nowEpoch = gRtcTime.epoch;
        xSemaphoreGive(rtcMutex);
    }
    if (nowEpoch >= hatchEpoch) return 0;
    return (int)((hatchEpoch - nowEpoch) / 86400UL);
}

bool checkMilestone(uint32_t nowEpoch, char* outLabel, size_t labelLen) {
    uint32_t startEpoch  = 0;
    uint16_t lockdownDay = 0;
    uint16_t totalDays   = 0;

    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        startEpoch  = gSettings.startEpoch;
        lockdownDay = gSettings.lockdownDay;
        totalDays   = gSettings.totalDays;
        xSemaphoreGive(settingsMutex);
    }
    if (startEpoch == 0 || totalDays == 0) return false;
    if (!rtcEpochValid) return false;

    EggType eggType = EGG_CHICKEN;
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        eggType = gSettings.eggType;
        xSemaphoreGive(settingsMutex);
    }

    int day = (int)((nowEpoch - startEpoch) / 86400UL) + 1;

    // Fertile candling day (per egg type)
    int candleDay = CHICKEN_CANDLE_DAY;
    if      (eggType == EGG_DUCK)  candleDay = DUCK_CANDLE_DAY;
    else if (eggType == EGG_QUAIL) candleDay = QUAIL_CANDLE_DAY;
    if (day == candleDay) {
        snprintf(outLabel, labelLen, "Candle Day!");
        return true;
    }
    if (day >= (int)lockdownDay && day < (int)totalDays) {
        snprintf(outLabel, labelLen, "LOCKDOWN");
        return true;
    }
    if (day == (int)totalDays) {
        snprintf(outLabel, labelLen, "HATCH DAY!");
        return true;
    }
    return false;
}

bool turnerActive(void) {
    uint32_t startEpoch  = 0;
    uint16_t lockdownDay = 0;
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        startEpoch  = gSettings.startEpoch;
        lockdownDay = gSettings.lockdownDay;
        xSemaphoreGive(settingsMutex);
    }
    if (startEpoch == 0) return false;
    int day = calcIncubationDay();
    return (day > 0 && day < (int)lockdownDay);
}
