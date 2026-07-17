# User Stories — Incubators

**Priority:** P1 · **Surfaces:** Both (status), Web (configuration)

## US-INC-001: Register an incubator [Web]
As the Owner, I want to create an incubator record (name, capacity, default species) and bind a provisioned device to it.
- Given a provisioned device, when I bind it, then telemetry appears under this incubator. Rules: BR-007

## US-INC-002: Live incubator status [Both]
As a user, I want current temperature, humidity, turner state, day-of-incubation, and active batch at a glance.
- Given the device is online, when I open the incubator, then readings are ≤60 s old and the active batch's day/schedule is shown.
- Given no active batch, then status shows readings with "no active batch".

## US-INC-003: Configure setpoints with acknowledgement states [Web]
As the Owner, I want to change setpoints/thresholds and see sent → received → applied states, so I know the device actually applied them.
- Given I save a config change, when it is dispatched, then the UI shows the config version and its ack state per the iot contract; `applied` only after device confirmation.
- Given no ack within the retry window, then the change shows `unconfirmed` and a warning is raised.

## US-INC-004: Incubator status over BLE when WiFi is down [Android]
As the Owner standing at the incubator, I want live readings over Bluetooth when MQTT is unreachable.
- Given the device is MQTT-offline but in BLE range, when I open it in the app, then current readings display marked "local (BLE)" and are not written to server history directly (they sync as offline-captured records per BR-010).
Rules: ADR 0002
