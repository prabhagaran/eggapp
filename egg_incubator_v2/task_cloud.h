#ifndef TASK_CLOUD_H
#define TASK_CLOUD_H

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
// Cloud Communication Task
// ─────────────────────────────────────────────────────────────
//
// Handles:
// - WiFi connectivity monitoring
// - Cloud data upload
// - Remote commands
// - OTA triggers (optional future)
// - Sync with dashboard/server
//
void task_cloud(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif // TASK_CLOUD_H
