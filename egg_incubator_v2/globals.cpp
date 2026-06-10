#include "globals.h"
#include "config.h"
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// SHARED STATE
// ─────────────────────────────────────────────────────────────────────────────
SensorData_t  gSensorData  = { 0.0f, 0.0f, false, false };
RtcTime_t     gRtcTime     = {};
RelayState_t  gRelayState  = { false, false, false, false, false, false };
Settings_t    gSettings    = {};

// ─────────────────────────────────────────────────────────────────────────────
// SYNCHRONIZATION PRIMITIVES
// ─────────────────────────────────────────────────────────────────────────────
SemaphoreHandle_t sensorMutex   = nullptr;
SemaphoreHandle_t rtcMutex      = nullptr;
SemaphoreHandle_t controlMutex  = nullptr;
SemaphoreHandle_t settingsMutex = nullptr;
QueueHandle_t      uiEventQueue   = nullptr;
QueueHandle_t      errorQueue     = nullptr;
QueueHandle_t      telemetryQueue = nullptr;
EventGroupHandle_t suspendAckGroup = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// FAULT STATE — written/read under faultMux critical section
// ─────────────────────────────────────────────────────────────────────────────
volatile bool overTempFault  = false;
portMUX_TYPE  faultMux       = portMUX_INITIALIZER_UNLOCKED;

std::atomic<bool> wifiUserEnabled{false};  // true = user has enabled Wi-Fi
std::atomic<bool> wifiPortalActive{false}; // true = config portal is running
volatile bool rtcEpochValid    = false;  // true = RTC epoch > Nov 2023

// ─────────────────────────────────────────────────────────────────────────────
// TASK HANDLES — needed for suspend/resume on profile switch
// ─────────────────────────────────────────────────────────────────────────────
TaskHandle_t hTaskTurner        = nullptr;
TaskHandle_t hTaskFan           = nullptr;
TaskHandle_t hTaskPump          = nullptr;
TaskHandle_t hTaskMilestone     = nullptr;
TaskHandle_t hTaskTempControl   = nullptr;
TaskHandle_t hTaskClimateControl= nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// pushError — safe from any task, non-blocking
// ─────────────────────────────────────────────────────────────────────────────
void pushError(const char* type, const char* message) {
    ErrorMsg_t err;
    strncpy(err.type,    type,    sizeof(err.type)    - 1);
    strncpy(err.message, message, sizeof(err.message) - 1);
    err.type[sizeof(err.type) - 1]       = '\0';
    err.message[sizeof(err.message) - 1] = '\0';
    // Non-blocking: if queue is full, the error is silently dropped
    xQueueSend(errorQueue, &err, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// setRelay — single point of relay control; mirrors state into gRelayState
// ─────────────────────────────────────────────────────────────────────────────
void setRelay(uint8_t pin, bool on) {
    // GPIO write is atomic on ESP32 — always execute it regardless of mutex state.
    // The mutex only protects the gRelayState software mirror.
    digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);

    if (xSemaphoreTake(controlMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        switch (pin) {
            case RELAY_HEATER:     gRelayState.heaterOn     = on; break;
            case RELAY_COOLER:     gRelayState.coolerOn     = on; break;
            case RELAY_HUMIDIFIER: gRelayState.humidifierOn = on; break;
            case RELAY_PUMP:       gRelayState.pumpOn       = on; break;
            case RELAY_TURNER:     gRelayState.turnerOn     = on; break;
        }
        xSemaphoreGive(controlMutex);
    } else {
        pushError("FAULT", "setRelay: controlMutex timeout");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// allRelaysOff — emergency / boot / profile-switch safe shutdown
// ─────────────────────────────────────────────────────────────────────────────
void allRelaysOff(void) {
    setRelay(RELAY_HEATER,     false);
    setRelay(RELAY_COOLER,     false);
    setRelay(RELAY_HUMIDIFIER, false);
    // Use PWM helper to stop the fan (do not call digitalWrite on PWM pin)
    setFanSpeed(0);
    setRelay(RELAY_PUMP,       false);
    setRelay(RELAY_TURNER,     false);
}
