#ifndef TASK_BUTTONS_H
#define TASK_BUTTONS_H

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
// Button Task
// ─────────────────────────────────────────────────────────────

// FreeRTOS task that handles UI buttons
void task_buttons(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif // TASK_BUTTONS_H