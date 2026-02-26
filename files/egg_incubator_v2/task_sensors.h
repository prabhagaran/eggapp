#ifndef TASK_SENSORS_H
#define TASK_SENSORS_H

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
// FreeRTOS Sensor Tasks
// ─────────────────────────────────────────────────────────────

// DS18B20 temperature task
void task_ds18b20(void* pvParameters);

// DHT humidity task
void task_sensor(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif // TASK_SENSORS_H