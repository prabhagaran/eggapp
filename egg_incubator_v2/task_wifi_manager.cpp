#include "task_wifi_manager.h"
#include "globals.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <Arduino.h>
#include <atomic>
#include <time.h>
#include "RTClib.h"

extern RTC_DS1307 rtc;  // defined in egg_incubator_v2.ino

// ─────────────────────────────────────────────────────────────────────────────
// Module-private state — only task_wifi_manager touches wm directly.
// UI posts requests via wifiReqPending; the task executes them on its own core.
// ─────────────────────────────────────────────────────────────────────────────
static WiFiManager wm;

static constexpr int WIFI_REQ_NONE       = 0;
static constexpr int WIFI_REQ_CONNECT    = 1;
static constexpr int WIFI_REQ_DISCONNECT = 2;
static constexpr int WIFI_REQ_NTP        = 3;
static std::atomic<int> wifiReqPending{WIFI_REQ_NONE};

// NTP result: 0=idle/pending, 1=success, 2=timeout, 3=no WiFi
static std::atomic<int> ntpSyncResult{0};

// ─────────────────────────────────────────────────────────────────────────────
// wifi_request_connect / wifi_request_disconnect
//
// Safe to call from any task — they only set an atomic flag.
// The actual WiFiManager calls execute inside task_wifi_manager on Core 0.
// ─────────────────────────────────────────────────────────────────────────────
void wifi_request_connect(void) {
    wifiReqPending.store(WIFI_REQ_CONNECT, std::memory_order_release);
}

void wifi_request_disconnect(void) {
    wifiReqPending.store(WIFI_REQ_DISCONNECT, std::memory_order_release);
}

void wifi_request_ntp_sync(void) {
    ntpSyncResult.store(0, std::memory_order_release);  // clear previous result
    wifiReqPending.store(WIFI_REQ_NTP, std::memory_order_release);
}

// Returns 0=pending, 1=success, 2=timeout, 3=no WiFi
int wifi_get_ntp_result(void) {
    return ntpSyncResult.load(std::memory_order_acquire);
}

// ─────────────────────────────────────────────────────────────────────────────
// task_wifi_manager
//
// FreeRTOS task — runs on Core 0 at the lowest priority.
// Three responsibilities:
//   1. While portal is active: call wm.process() every ~100 ms (non-blocking).
//      Close portal once connected.
//   2. If wifiUserEnabled && not connected: attempt WiFi.reconnect() with a
//      5-second back-off.
//   3. Otherwise: sleep for 2 seconds before next check.
//
// NEVER blocks other tasks. NEVER starts the portal automatically.
// ─────────────────────────────────────────────────────────────────────────────
void task_wifi_manager(void* pvParameters) {

    // ── Boot check: WiFi.begin() at startup may already have connected ──────
    // Give the stack up to 8 s to associate before declaring offline.
    {
        uint32_t deadline = millis() + 8000;
        while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        if (WiFi.status() == WL_CONNECTED) {
            wifiUserEnabled.store(true, std::memory_order_release);
            Serial.print("[WIFI] Auto-connected at boot: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("[WIFI] No stored credentials or connection failed — offline");
        }
    }

    for (;;) {

        // ── Consume pending UI request (connect / disconnect) ─────────────
        int req = wifiReqPending.exchange(WIFI_REQ_NONE, std::memory_order_acq_rel);

        if (req == WIFI_REQ_CONNECT && !wifiPortalActive.load(std::memory_order_acquire)) {
            WiFi.mode(WIFI_STA);
            wm.setConfigPortalBlocking(false);
            wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SEC);
            WiFi.setAutoReconnect(true);
            WiFi.persistent(true);
            Serial.println("[WIFI] Trying stored credentials (user-requested)...");
            bool connected = wm.autoConnect("INCUBATOR_SETUP");
            if (connected) {
                wifiUserEnabled.store(true, std::memory_order_release);
                wifiPortalActive.store(false, std::memory_order_release);
                Serial.print("[WIFI] Connected from stored creds: ");
                Serial.println(WiFi.localIP());
            } else {
                wifiUserEnabled.store(true, std::memory_order_release);
                wifiPortalActive.store(true, std::memory_order_release);
                Serial.println("[WIFI] Portal open: join 'INCUBATOR_SETUP' to configure");
            }
        } else if (req == WIFI_REQ_NTP) {
            if (WiFi.status() != WL_CONNECTED) {
                ntpSyncResult.store(3, std::memory_order_release);
                Serial.println("[NTP] WiFi not connected");
            } else {
                configTime(NTP_UTC_OFFSET_SEC, 0, NTP_SERVER);
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 5000)) {
                    rtc.adjust(DateTime(
                        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
                    Serial.printf("[NTP] RTC updated: %04d-%02d-%02d %02d:%02d:%02d\n",
                        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                    ntpSyncResult.store(1, std::memory_order_release);
                } else {
                    Serial.println("[NTP] getLocalTime timed out");
                    ntpSyncResult.store(2, std::memory_order_release);
                }
            }
        } else if (req == WIFI_REQ_DISCONNECT) {
            wifiUserEnabled.store(false, std::memory_order_release);
            wifiPortalActive.store(false, std::memory_order_release);
            wm.stopConfigPortal();
            WiFi.setAutoReconnect(false);
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            Serial.println("[WIFI] Disconnected by user");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // ── Portal processing ─────────────────────────────────────────────
        if (wifiPortalActive.load(std::memory_order_acquire)) {
            wm.process();   // non-blocking — hands control back immediately

            if (WiFi.status() == WL_CONNECTED) {
                wifiPortalActive.store(false, std::memory_order_release);
                wifiUserEnabled.store(true, std::memory_order_release);
                Serial.print("[WIFI] Connected: ");
                Serial.println(WiFi.localIP());
            } else if (!wm.getConfigPortalActive()) {
                // Portal timed out without a connection
                wifiPortalActive.store(false, std::memory_order_release);
                wifiUserEnabled.store(false, std::memory_order_release);
                Serial.println("[WIFI] Portal timed out — staying offline");
            }

            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // ── Auto-reconnect (only if user previously connected) ────────────
        if (wifiUserEnabled.load(std::memory_order_acquire) && WiFi.status() != WL_CONNECTED) {
            Serial.println("[WIFI] Reconnecting...");
            WiFi.reconnect();
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // ── Idle ──────────────────────────────────────────────────────────
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
