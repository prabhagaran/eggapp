#include "task_wifi_manager.h"
#include "globals.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// Module-private WiFiManager instance
// ─────────────────────────────────────────────────────────────────────────────
static WiFiManager wm;

// ─────────────────────────────────────────────────────────────────────────────
// wifi_request_connect
//
// Called from task_ui when the user selects "Connect" in the WiFi menu.
// Opens a non-blocking captive-portal AP named "INCUBATOR_SETUP".
// Sets wifiPortalActive = true so the task loop calls wm.process() each cycle.
// ─────────────────────────────────────────────────────────────────────────────
void wifi_request_connect(void) {
    if (wifiPortalActive.load(std::memory_order_acquire)) return;  // portal already running, ignore

    WiFi.mode(WIFI_STA);
    wm.setConfigPortalBlocking(false);   // non-blocking — must call process() in loop
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SEC);

    // Ensure the WiFi stack may auto-reconnect while the user requests connect
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    // autoConnect() first tries stored credentials silently;
    // only opens the captive-portal AP if that fails.
    Serial.println("[WIFI] Trying stored credentials (user-requested)...");
    bool connected = wm.autoConnect("INCUBATOR_SETUP");

    if (connected) {
        // Stored credentials worked — no portal needed
        wifiUserEnabled.store(true, std::memory_order_release);
        wifiPortalActive.store(false, std::memory_order_release);
        Serial.print("[WIFI] Connected from stored creds: ");
        Serial.println(WiFi.localIP());
    } else {
        // Could not connect — portal AP is now open (non-blocking)
        wifiUserEnabled.store(true, std::memory_order_release);
        wifiPortalActive.store(true, std::memory_order_release);
        Serial.println("[WIFI] Portal open: join 'INCUBATOR_SETUP' to configure");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// wifi_request_disconnect
//
// Called from task_ui when the user selects "Disconnect" in the WiFi menu.
// Stops the portal if active, disconnects, and turns WiFi radio off.
// ─────────────────────────────────────────────────────────────────────────────
void wifi_request_disconnect(void) {
    wifiUserEnabled.store(false, std::memory_order_release);
    wifiPortalActive.store(false, std::memory_order_release);
    wm.stopConfigPortal();
    // Disable automatic reconnect at the WiFi driver level so saved
    // credentials are not re-applied until the user explicitly requests.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[WIFI] Disconnected by user");
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
