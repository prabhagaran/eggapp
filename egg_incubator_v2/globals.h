#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include "RTClib.h"
#include "config.h"
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// ENUMS
// ─────────────────────────────────────────────────────────────────────────────

enum ProfileType : uint8_t {
    PROFILE_EGG_INCUBATOR = 0,
    PROFILE_CLIMATE_CHAMBER
};

enum ControlMode : uint8_t {
    MODE_AUTO = 0,
    MODE_MANUAL
};

enum EggType : uint8_t {
    EGG_CHICKEN = 0,
    EGG_DUCK,
    EGG_QUAIL,
    EGG_CUSTOM,
    EGG_TYPE_COUNT
};

enum ClimateModeType : uint8_t {
    CLIMATE_FIXED_SCHEDULE = 0,
    CLIMATE_CYCLIC,
    CLIMATE_RAMP
};

// ─────────────────────────────────────────────────────────────────────────────
// SENSOR DATA (protected by sensorMutex)
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    float  temp_ds18b20;   // °C  — -999.0 means sensor fault
    float  humidity_dht;   // %RH — last valid value retained on NaN
    float  temp_dht;       // °C  — DHT22 temperature channel for plausibility cross-check
    bool   temp_valid;
    bool   hum_valid;
    bool   dht_temp_valid; // true when temp_dht is a fresh, in-range reading
} SensorData_t;

// ─────────────────────────────────────────────────────────────────────────────
// RTC DATA (protected by rtcMutex)
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    DateTime now;
    uint32_t epoch;
} RtcTime_t;

// ─────────────────────────────────────────────────────────────────────────────
// RELAY / OUTPUT STATE (protected by controlMutex)
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    bool heaterOn;
    bool coolerOn;
    bool humidifierOn;
    bool fanOn;
    bool pumpOn;
    bool turnerOn;
} RelayState_t;

// ─────────────────────────────────────────────────────────────────────────────
// ERROR MESSAGE (sent over errorQueue)
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    char type[20];
    char message[60];
} ErrorMsg_t;

// ─────────────────────────────────────────────────────────────────────────────
// RAMP STEP (Climate Chamber ramp profile)
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    uint32_t durationMin;   // how long to hold this temperature
    float    targetTemp;    // target temperature for this step
} RampStep_t;

// ─────────────────────────────────────────────────────────────────────────────
// INCUBATOR SETTINGS (protected by settingsMutex)
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    // Common
    ProfileType   activeProfile;
    ControlMode   controlMode;

    // Temperature / humidity
    float         tempSetpoint;
    float         tempHysteresis;
    float         humSetpoint;
    float         humHysteresis;

    // Manual overrides
    bool          heaterManualOn;
    bool          coolerManualOn;

    // Egg incubator
    EggType       eggType;
    uint16_t      totalDays;
    uint16_t      lockdownDay;
    float         lockdownHumidity;
    uint32_t      startEpoch;         // 0 = not started
    uint16_t      turnerIntervalMin;
    uint16_t      turnerDurationSec;
    uint32_t      lastTurnEpoch;
    // fanIntervalMin and fanDurationSec removed — PWM speed-only control
    uint8_t       fanSpeedPercent;
    uint16_t      pumpDurationSec;

    // Climate chamber
    ClimateModeType climateMode;
    uint8_t         schedStartHour;
    uint8_t         schedEndHour;
    uint32_t        heatPeriodMin;
    uint32_t        coolPeriodMin;
    uint32_t        cycleStartEpoch;
    uint8_t         rampCount;
    RampStep_t      rampSteps[CLIMATE_MAX_RAMP_STEPS];
    uint8_t         rampStepIdx;
    uint32_t        rampStepStartEpoch;
} Settings_t;

// ─────────────────────────────────────────────────────────────────────────────
// UI EVENTS
// ─────────────────────────────────────────────────────────────────────────────
enum UiEvent : uint8_t {
    UI_EVT_NONE,
    UI_EVT_UP,
    UI_EVT_DOWN,
    UI_EVT_OK,
    UI_EVT_LONGOK
};

// ─────────────────────────────────────────────────────────────────────────────
// UI STATES
// ─────────────────────────────────────────────────────────────────────────────
enum UiState : uint8_t {
    UI_HOME,
    UI_FAULT,

    UI_MAIN_MENU,
    UI_CONTROLLER_MODE_MENU,

    UI_SET_ENV_MENU,
    UI_ENV_TEMPERATURE,
    UI_ENV_HUMIDITY,
    UI_ENV_HYSTERESIS_MENU,
    UI_ENV_HYST_TEMP_EDIT,
    UI_ENV_HYST_HUM_EDIT,
    UI_ENV_EGG_TYPE,
    UI_ENV_INCUBATION_DAY,
    UI_ENV_TURNER,
    UI_ENV_FAN,

    // Profile-grouped setting menus
    UI_EGG_INCUBATOR_MENU,
    UI_CLIMATE_CHAMBER_MENU,
    UI_SYSTEM_MENU,
    UI_PUMP_SETTINGS,

    UI_CLIMATE_MODE_MENU,
    UI_CLIMATE_SCHEDULE,
    UI_CLIMATE_CYCLIC,
    UI_CLIMATE_RAMP,

    UI_SETTINGS_MENU,
    UI_WIFI_MENU,
    UI_MODE_MENU,
    UI_MANUAL_CONTROL_MENU,
    UI_DEVICE_INFO,
    UI_FACTORY_RESET_CONFIRM,

    // Time & Date editing states
    UI_TIME_DATE_MENU,      // submenu: Manual Set / WiFi Sync / Back
    UI_TIME_MANUAL_EDIT,    // step-by-step field editor
    UI_TIME_WIFI_SYNC,      // NTP sync progress / result screen
};

// ─────────────────────────────────────────────────────────────────────────────
// MENU INDEX ENUMS
// ─────────────────────────────────────────────────────────────────────────────
enum MainMenuItem : uint8_t {
    MENU_EGG_INCUBATOR = 0,
    MENU_CLIMATE_CHAMBER,
    MENU_SYSTEM,
    MENU_BACK,
    MENU_COUNT
};

enum EnvMenuItem : uint8_t {
    ENV_TEMPERATURE = 0,
    ENV_HYSTERESIS,
    ENV_HUMIDITY,
    ENV_INCUBATION_DAY,   // incubator only (hidden in climate mode)
    ENV_EGG_TYPE,         // incubator only
    ENV_TURNER,           // incubator only
    ENV_FAN,              // incubator only
    ENV_CLIMATE_MODE,     // climate only
    ENV_BACK,
    ENV_COUNT
};

enum SettingsMenuItem : uint8_t {
    SET_TIME_DATE = 0,
    SET_WIFI,
    SET_MODE,
    SET_DEVICE_INFO,
    SET_FACTORY_RESET,
    SET_BACK,
    SET_COUNT
};

// ─────────────────────────────────────────────────────────────────────────────
// EXTERN DECLARATIONS — defined in globals.cpp
// ─────────────────────────────────────────────────────────────────────────────
extern SensorData_t  gSensorData;
extern RtcTime_t     gRtcTime;
extern RelayState_t  gRelayState;
extern Settings_t    gSettings;

extern SemaphoreHandle_t sensorMutex;
extern SemaphoreHandle_t rtcMutex;
extern SemaphoreHandle_t controlMutex;
extern SemaphoreHandle_t settingsMutex;

extern QueueHandle_t uiEventQueue;
extern QueueHandle_t errorQueue;

// Telemetry message for queued uploads (fixed-size to avoid heap alloc)
typedef struct {
    char     url[768];
    uint8_t  attempts;
    uint32_t nextAttemptMs;
} TelemetryMsg_t;

extern QueueHandle_t telemetryQueue;

extern volatile bool overTempFault;
extern portMUX_TYPE  faultMux;

extern std::atomic<bool> wifiUserEnabled;   // true = user has enabled Wi-Fi
extern std::atomic<bool> wifiPortalActive;  // true = config portal is running
extern volatile bool rtcEpochValid;     // true = RTC epoch is sane (> Nov 2023)

extern TaskHandle_t hTaskTurner;
extern TaskHandle_t hTaskFan;
extern TaskHandle_t hTaskPump;
extern TaskHandle_t hTaskMilestone;
extern TaskHandle_t hTaskTempControl;
extern TaskHandle_t hTaskClimateControl;
extern SemaphoreHandle_t  milestoneMutex;   // guards gMilestoneLabel; created in setup()
extern EventGroupHandle_t suspendAckGroup;  // task suspend ack bits — F-01 fix

// ─────────────────────────────────────────────────────────────────────────────
// HELPER: push an error onto errorQueue (safe from any task)
// ─────────────────────────────────────────────────────────────────────────────
void pushError(const char* type, const char* message);

// ─────────────────────────────────────────────────────────────────────────────
// HELPER: round a float to one decimal place (avoids IEEE 754 accumulation)
// ─────────────────────────────────────────────────────────────────────────────
inline float round1(float v) { return roundf(v * 10.0f) / 10.0f; }

// ─────────────────────────────────────────────────────────────────────────────
// HELPER: safe relay write — ALL relay changes must go through this
// ─────────────────────────────────────────────────────────────────────────────
void setRelay(uint8_t pin, bool on);
void allRelaysOff(void);
void clampEpochsToNow(uint32_t newNow);  // call after rtc.adjust() to prevent epoch underflow
// LEDC-based fan PWM helper (implemented in task_incubator.cpp)
void setFanSpeed(uint8_t percent);

#endif // GLOBALS_H
