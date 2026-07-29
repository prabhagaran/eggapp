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
//
// BLOCKS until the node is on a network. It first offers a short window to
// press BOOT (which forces the setup portal even when a good network is
// stored), then joins the stored network, raising the captive portal if
// there is none or it is unreachable — retrying forever.
//
// Blocking is intentional: a coop node has nothing useful to do without a
// network, and the non-blocking portal needs process() pumped from loop()
// on a tight cadence, which makes the setup form unreliable to submit.
void wifiManagerBegin(void);

// Call every loop(). Reconnects with back-off if the link drops. Returns
// true when the node is associated and has an IP. Never reopens the
// portal by itself — only the BOOT button at startup does that.
bool wifiManagerLoop(void);

#endif // WIFI_MANAGER_H
