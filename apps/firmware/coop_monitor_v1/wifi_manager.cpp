#include "wifi_manager.h"
#include <WiFi.h>
#include <WiFiManager.h>

// Module-private — nothing else touches WiFiManager directly.
static WiFiManager   wm;
static unsigned long lastReconnectMs = 0;

// Back-off between reconnect attempts once we have been provisioned.
// Deliberately retries the stored network forever rather than escalating
// back to AP mode: a coop node sitting in AP mode has stopped publishing
// and, from the dashboard, looks identical to one that died. A human with
// the BOOT button is the only thing that reopens the portal.
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
    Serial.printf("[WiFi] Press BOOT within %d s (hold %d s) to re-run WiFi setup...\n",
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

static void printPortalBanner(void) {
    Serial.println();
    Serial.println("  ┌──────────────────────────────────────────────┐");
    Serial.printf("  │  WiFi setup — join AP:  %-20s │\n", WIFI_PORTAL_AP_NAME);
    Serial.println("  │  Then open:  http://192.168.4.1              │");
    Serial.println("  │  Android: tap 'Stay connected' if it warns   │");
    Serial.println("  │  about no internet, or it will drop the AP.  │");
    Serial.println("  └──────────────────────────────────────────────┘");
    Serial.println();
}

void wifiManagerBegin(void) {
    WiFi.mode(WIFI_STA);

    // BLOCKING portal. setup() does not return until the node is on a
    // network — unlike the incubator sketch, which must keep its control
    // loop running regardless of WiFi because there are eggs under a
    // heater. A coop node has nothing to do without a network, and a
    // blocking portal is far more reliable to actually fill in: the
    // non-blocking variant needs process() pumped continuously from
    // loop(), and any hiccup in that cadence makes the setup form fail
    // to submit.
    wm.setConfigPortalBlocking(true);
    wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_SEC);
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SEC);
    wm.setTitle("Coop Monitor Setup");
    wm.setBreakAfterConfig(true);   // leave the portal as soon as creds are saved

    const char* pass = strlen(WIFI_PORTAL_AP_PASSWORD) ? WIFI_PORTAL_AP_PASSWORD : nullptr;

    if (portalRequestedByButton()) {
        Serial.println("[WiFi] BOOT pressed — starting WiFi setup portal");
        printPortalBanner();
        wm.startConfigPortal(WIFI_PORTAL_AP_NAME, pass);   // blocks
    }

    // Retry forever until associated. autoConnect() joins the stored
    // network, and raises the portal itself when there is none or it is
    // unreachable. On portal timeout it returns false and we go round
    // again — which re-tries the stored network first, so a router that
    // was merely slow coming back after a power cut recovers on its own
    // without anyone touching the device.
    unsigned int attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
        attempt++;
        Serial.printf("[WiFi] Connect attempt %u — trying stored network, "
                      "portal \"%s\" if unreachable\n", attempt, WIFI_PORTAL_AP_NAME);
        if (attempt == 1 || attempt % 5 == 0) printPortalBanner();

        if (wm.autoConnect(WIFI_PORTAL_AP_NAME, pass)) break;   // blocks

        Serial.println("[WiFi] Not connected yet — retrying");
        delay(1000);
    }

    Serial.printf("[WiFi] Connected to \"%s\", IP %s\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

bool wifiManagerLoop(void) {
    if (WiFi.status() == WL_CONNECTED) return true;

    // Provisioned but the link dropped. Reconnect quietly in the
    // background; never reopen the portal on its own (see the note on
    // RECONNECT_BACKOFF_MS above).
    if (millis() - lastReconnectMs >= RECONNECT_BACKOFF_MS) {
        lastReconnectMs = millis();
        Serial.println("[WiFi] Link lost — reconnecting");
        WiFi.reconnect();
    }
    return false;
}
