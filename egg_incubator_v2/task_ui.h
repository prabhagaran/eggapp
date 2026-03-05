#ifndef TASK_UI_H
#define TASK_UI_H

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
// UI Task
// ─────────────────────────────────────────────────────────────

// FreeRTOS task that runs the complete UI state machine
void task_ui(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif // TASK_UI_H