# MVP Definition

## Introduction

This document defines the **Minimum Viable Product (MVP)** for the  
**Reusable Environmental Control Platform**.

The MVP represents the **first complete, usable, and testable system** that validates:
- Architecture
- Hardware choices
- Control logic
- UI design
- Safety handling

The MVP is intentionally limited in scope to reduce risk and complexity.

---

## What Is the MVP?

The MVP is a **Thermostat Profile–based implementation** of the platform.

It is not a prototype or demo, but a **fully functional subset** of the final system that:
- Runs on real hardware
- Uses the final architecture
- Can be extended without redesign

---

## Why Thermostat as MVP?

The Thermostat profile is chosen because it:
- Uses the core control loop (temperature)
- Requires both heating and cooling
- Exercises safety logic
- Validates UI and encoder interaction
- Avoids unnecessary early complexity

If the Thermostat works correctly, all other profiles can be built on top of it.

---

## MVP Scope

### Included Features

The MVP includes:

#### Hardware
- ESP32
- DS18B20 temperature sensor
- Relay-based heater control
- Relay-based cooler control
- OLED display
- Rotary encoder

#### Software
- FreeRTOS task architecture
- System Supervisor FSM
- Temperature Control FSM
- Alarm FSM
- Actuator HAL
- Sensor HAL
- Profile system (Thermostat only)
- Nokia-style UI
- Serial logging

---

### Excluded Features (Deferred)

The following features are **explicitly excluded** from the MVP:

- Humidity control
- Water level monitoring
- Rotation motor control
- Timer-based automation
- Wi-Fi or cloud connectivity
- Data storage (SD card)

These will be added in later phases once the MVP is stable.

---

## MVP Functional Requirements

The MVP must satisfy the following requirements:

- Maintain temperature within configured setpoint and hysteresis
- Never enable heater and cooler simultaneously
- Detect temperature sensor failure
- Enter safe state on critical faults
- Display system status on OLED
- Allow user to set temperature via encoder
- Require acknowledgment for alarms
- Log system activity via serial output

---

## MVP Safety Requirements

The MVP must guarantee:

- All actuators are OFF on startup
- All actuators are OFF during faults
- Sensor failures override control logic
- User input cannot bypass safety rules

Safety behavior must be deterministic and testable.

---

## MVP Validation Criteria

The MVP is considered complete when:

- The system boots reliably
- Temperature control behaves correctly
- UI navigation is responsive and consistent
- Alarms trigger and clear correctly
- No task blocks or starves others
- System runs continuously without crashes

Only after meeting these criteria can new features be added.

---

## MVP as a Foundation

Once the MVP is validated:
- Humidity control can be added
- Water level FSM can be integrated
- Additional profiles can be enabled
- UI menus can expand naturally
- Logging can be extended

No core architecture changes should be required.

---

## Summary

The MVP:
- Proves the platform architecture
- Minimizes early risk
- Establishes a stable foundation
- Enables confident expansion

The Thermostat MVP is the **anchor point** for the entire platform.

---

➡️ Next: **Development → Roadmap**
