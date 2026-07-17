#ifndef TASK_MQTT_H
#define TASK_MQTT_H

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
// Task: MQTT Telemetry
// ─────────────────────────────────────────────────────────────
//
// Runs alongside task_cloud (Google Sheets) — additive, not a
// replacement. Publishes the same telemetry snapshot to a local
// MQTT broker (see MQTT_* macros in config.h / secrets.h) so the
// eggAPP backend can consume it without depending on Google Sheets.
//
// Publishes:
//   <MQTT_TOPIC_PREFIX>/<DEVICE_ID>/telemetry  — JSON, every
//                                                 MQTT_TELEMETRY_INTERVAL_MS
//   <MQTT_TOPIC_PREFIX>/<DEVICE_ID>/status     — "online" (retained) on
//                                                 connect; "offline"
//                                                 (retained) via LWT on
//                                                 unexpected disconnect
//
// Entirely independent of task_cloud: a Google Sheets outage does not
// affect MQTT publishing and vice versa. Never blocks other tasks —
// same non-blocking WiFi-gated pattern as task_cloud.
void task_mqtt(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif // TASK_MQTT_H
