#ifndef TASK_CONTROL_H
#define TASK_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
// FreeRTOS Control Task
// ─────────────────────────────────────────────────────────────

// Temperature & humidity control task (egg incubator profile)
void task_temperature_control(void* pvParameters);

// Climate chamber control task (climate chamber profile)
void task_climate_control(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif // TASK_CONTROL_H