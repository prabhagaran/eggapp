#include "wifi_manager.h"
#include <WiFi.h>
#include <WiFiManager.h>

// Module-private — nothing else touches WiFiManager directly.
static WiFiManager wm;
static bool          portalActive     = false;
static unsigned long lastReconnectMs  = 0;

// Back-off between reconnect attempts. Deliberately unbounded in duration
// rather than escalating to "open the portal": a coop node that dropped
// into AP mode on its own would stop publishing and be indistinguishable
// from a dead one, and nobody is watching its serial output. It retries
// the stored network forever; a human with the BOOT button is the only
// thing that reopens the portal.
static const unsigned long RECONNECT_BACKOFF_MS = 10000UL;

// Watches the BOOT button for a sustained press during a short window
// AFTER startup — deliberately not "held at power-on".
//
// GPIO0 is an ESP32 strapping pin: pulled low at reset it puts the chip
// into serial download mode and the sketch never runs at all. So the
// trigger cannot be "hold BOOT while powering on" — it has to be "power
// on, then press and hold BOOT" once this code is already executing.
static bool portalRequestedByButton(void) {
    pinMode(PORTAL_TRIGGER_PIN, INPUT_PULLUP);
    Serial.printf("[WiFi] Press BOOT within %d s (hold %d s) to open the setup portal...\n",
                  PORTAL_TRIGGER_WINDOW_MS / 1000, PORTAL_TRIGGER_HOLD_MS / 1000);

    unsigned long windowEnd = millis() + PORTAL_TRIGGER_WINDOW_MS;
    while (millis() < windowEnd) {
        if (digitalRead(PORTAL_TRIGGER_PIN) == LOW) {
            // Require a sustained hold: a brief glitch on a strapping pin
            // must not wipe a working setup.
            unsigned long pressStart = millis();
            while (digitalRead(PORTAL_TRIGGER_PIN) == LOW) {
                if (millis() - pressStart >= PORTAL_TRIGGER_HOLD_MS) return true;
                delay(20);
            }
        }
        delay(20);
    }
    return false;
}

static void startPortal(const char* why) {
    Serial.printf("[WiFi] Opening config portal (%s)\n", why);
    Serial.printf("[WiFi] Join AP \"%s\" from a phone, then pick your network.\n",
                  WIFI_PORTAL_AP_NAME);
    portalActive = true;
    // Non-blocking: loop() drives wm.process(), so the sketch keeps
    // sampling sensors and stays responsive while the portal is up.
    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SEC);
    const char* pass = strlen(WIFI_PORTAL_AP_PASSWORD) ? WIFI_PORTAL_AP_PASSWORD : nullptr;
    wm.startConfigPortal(WIFI_PORTAL_AP_NAME, pass);
}

void wifiManagerBegin(void) {
    WiFi.mode(WIFI_STA);
    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SEC);
    wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_SEC);
    // Keep the portal's own AP name in the title bar of the captive page.
    wm.setTitle("Coop Monitor Setup");

    if (portalRequestedByButton()) {
        // Explicit user request — skip the stored network entirely.
        startPortal("BOOT button pressed");
        return;
    }

    // autoConnect joins the stored network, and raises the portal itself
    // if there is none or it can't be reached. With blocking disabled it
    // returns immediately in the portal case, which loop() then services.
    const char* pass = strlen(WIFI_PORTAL_AP_PASSWORD) ? WIFI_PORTAL_AP_PASSWORD : nullptr;
    bool connected = wm.autoConnect(WIFI_PORTAL_AP_NAME, pass);

    if (connected) {
        Serial.printf("[WiFi] Connected to %s, IP %s\n",
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        portalActive = false;
    } else {
        portalActive = wm.getConfigPortalActive();
        if (portalActive) {
            Serial.printf("[WiFi] No stored network reachable — portal open as \"%s\"\n",
                          WIFI_PORTAL_AP_NAME);
        }
    }
}

bool wifiPortalIsActive(void) {
    return portalActive;
}

bool wifiManagerLoop(void) {
    if (portalActive) {
        wm.process();   // non-blocking; returns immediately

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] Provisioned — connected to %s, IP %s\n",
                          WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            wm.stopConfigPortal();
            portalActive = false;
            return true;
        }

        // Portal hit its timeout with nothing entered. Fall back to
        // retrying the stored network rather than sitting in AP mode
        // forever — the usual cause is a router that was slow to come
        // back after a power cut, not a genuinely new network.
        if (!wm.getConfigPortalActive()) {
            Serial.println("[WiFi] Portal timed out — retrying stored network");
            portalActive = false;
            WiFi.mode(WIFI_STA);
            WiFi.begin();
        }
        return false;
    }

    if (WiFi.status() == WL_CONNECTED) return true;

    if (millis() - lastReconnectMs >= RECONNECT_BACKOFF_MS) {
        lastReconnectMs = millis();
        Serial.println("[WiFi] Disconnected — reconnecting");
        WiFi.reconnect();
    }
    return false;
}
