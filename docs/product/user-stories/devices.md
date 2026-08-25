# User Stories — Devices

**Priority:** P1 · **Surfaces:** Android (provisioning), Web (fleet management)

## US-DEV-001: Provision a new incubator device via BLE [Android]
As the Owner, I want to pair a new ESP32 incubator over Bluetooth so it joins my farm without touching config files.
- Given an unprovisioned device in pairing mode, when I scan from the app, then it appears with its hardware ID.
- Given I select it, when I complete pairing per `docs/iot/ble-pairing-protocol.md`, send WiFi credentials, and bind it to a new or existing incubator record, then the device connects to MQTT and shows `active` within 2 minutes.
- Given pairing fails (wrong PIN/out of range), when I retry, then the flow resumes without a stale half-provisioned record.
Rules: BR-007 · ADR 0002

## US-DEV-002: Device fleet health view [Web]
As the Owner, I want a device list with online/offline state, last-seen, firmware version, and applied config version, so I can spot problems.
- Given a device misses heartbeats past the timeout, when I view the list, then it shows `offline` with last-seen timestamp.

## US-DEV-003: Device-offline alert [Both]
As the Owner, I want an alert when a device goes silent, because a dead incubator fan is an emergency.
- Given heartbeat loss is detected, when offline state is entered, then a critical alert fires (push + panel) per BR-014.

## US-DEV-004: Decommission a device [Web]
As the Owner, I want to retire a device so its credentials are revoked.
- Given a bound device, when I decommission it, then it is unbound, its MQTT credentials are revoked, and subsequent telemetry is rejected per BR-007.

## US-DEV-005: Update device firmware over the air [Web]
As the Owner, I want to push a firmware update to a device from the dashboard, so fixing a firmware bug does not mean carrying a laptop and USB cable to every incubator.
- Given a device is online, when I publish a firmware update to it, then it downloads the image, verifies it, applies it, reboots, and reports its new `fw` version in telemetry — with progress visible in the UI throughout.
- Given the download or verification fails, when the device reboots, then it comes back on its **previous** firmware and reports the failure — a failed update must never leave an incubator unable to run its control loop.
- Given a device is mid-update, when it is running an active batch, then temperature/humidity control is not suspended for longer than the reboot itself.
- Given an update is offered, when the image is not intended for that device type, then the device rejects it rather than bricking itself (a coop image must not install on an incubator).
Rules: BR-015 · Blocked on firmware support — see note below.

> **Not yet possible in firmware (2026-08-22).** `apps/firmware/egg_incubator_v2`
> implements no OTA path: there is no `ArduinoOTA`, `Update.h`, or HTTPS-OTA
> code. The partition scheme already reserves dual OTA slots
> (`PartitionScheme=min_spiffs`, 1.9 MB app), so the layout does not block
> this — only the firmware-side implementation and the delivery mechanism
> are missing. Every firmware change to date has required physical USB
> access to each board. Owned by **iot-integration-architect** (device
> contract) with **embedded-engineer** (firmware implementation); the
> delivery channel (MQTT-triggered pull vs. HTTP endpoint) is a contract
> decision that needs an ADR before implementation.
