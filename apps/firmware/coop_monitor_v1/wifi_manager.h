#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include "config.h"

// Phone-based WiFi provisioning via a captive portal (tzapu WiFiManager),
// the same library and approach the incubator sketch uses.
//
// Credentials live in WiFiManager's own NVS namespace, never in secrets.h
// and never in the compiled image.

// Call once from setup(), before anything that needs the network.
// Reads the BOOT button first: held at power-on, it forces the portal even
// when a good network is already stored. Otherwise it joins the stored
// network, and only raises the portal if that fails.
void wifiManagerBegin(void);

// Call every loop(). Services the non-blocking portal when it is open and
// reconnects with back-off when the link drops. Returns true when the node
// is associated and has an IP.
bool wifiManagerLoop(void);

// True while the captive portal is being served. Publishing is skipped in
// this state — the radio is running an AP, not a station.
bool wifiPortalIsActive(void);

#endif // WIFI_MANAGER_H
