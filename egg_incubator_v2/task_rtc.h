#ifndef TASK_RTC_H
#define TASK_RTC_H

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
// RTC Task
// ─────────────────────────────────────────────────────────────

// FreeRTOS task that polls RTC every second
void task_rtc(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif // TASK_RTC_H
