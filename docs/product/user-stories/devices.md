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
