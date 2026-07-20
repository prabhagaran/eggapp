#pragma once

#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// TASK: Wi-Fi Manager
//
// Responsibilities:
//   - Runs wm.process() every ~100 ms while portal is active (non-blocking).
//   - Monitors WiFi.status() and sets wifiPortalActive = false on connect.
//   - Reconnects automatically only if wifiUserEnabled == true.
//   - Never blocks any other task.
//
// Public API (called from task_ui.cpp):
//   wifi_request_connect()    — starts non-blocking config portal
//   wifi_request_disconnect() — disconnects and turns WiFi off
// ─────────────────────────────────────────────────────────────────────────────

void task_wifi_manager(void* pvParameters);

// Called from UI when user navigates Settings → WiFi → Connect
void wifi_request_connect(void);

// Called from UI when user navigates Settings → WiFi → Disconnect
void wifi_request_disconnect(void);

// Post an NTP sync request; wifi task executes it and updates the RTC.
void wifi_request_ntp_sync(void);

// Poll result: 0=pending, 1=success, 2=timeout, 3=no WiFi
int wifi_get_ntp_result(void);
