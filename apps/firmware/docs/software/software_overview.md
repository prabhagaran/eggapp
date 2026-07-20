# Software Overview

!!! note "Design-phase document"
    Written before implementation (Phase 0). Most concepts still apply, but
    some details differ in the as-built firmware (v2.0.0): input is three push
    buttons (no rotary encoder), there is no water-level sensor, control logic
    is task-based rather than formal FSMs, and Wi-Fi + cloud telemetry are
    implemented. Pages outside the *Design Archive* section reflect the
    current firmware.


## Introduction

This document provides an overview of the **software architecture** used in the  
**Reusable Environmental Control Platform**.

The software is designed to be:
- Modular
- Deterministic
- Reusable
- Easy to extend and maintain

All software components align with the system architecture, hardware design, and long-term product vision.

---

## Software Design Goals

The primary goals of the software design are:

- Clear separation of responsibilities
- Predictable execution using RTOS principles
- Hardware-independent control logic
- Profile-driven behavior
- Safety-first operation

The software avoids monolithic designs and tightly coupled code.

---

## Execution Environment

### Platform
- **Microcontroller:** ESP32
- **Framework:** Arduino core for ESP32
- **RTOS:** FreeRTOS (built-in with ESP32 Arduino core)

The Arduino IDE is used for development, while FreeRTOS provides structured task scheduling and communication.

---

## High-Level Software Architecture

+-----------------------------+
| User Interface (OLED + ENC) |
+-------------+---------------+
|
v
+-----------------------------+
| UI FSM & Command Layer |
+-------------+---------------+
|
v
+-----------------------------+
| Control FSMs (System, Temp, |
| Humidity, Water, Alarm) |
+-------------+---------------+
|
v
+-----------------------------+
| HAL (Sensors & Actuators) |
+-------------+---------------+
|
v
+-----------------------------+
| ESP32 Hardware |
+-----------------------------+


Each layer communicates only through defined interfaces.

---

## FreeRTOS-Based Task Structure

The software uses FreeRTOS to separate concerns into independent tasks.

| Task | Responsibility |
|----|----|
| Sensor Task | Read and validate sensor data |
| Control Task | Execute FSM logic and decisions |
| Actuator Task | Apply control decisions to hardware |
| UI Task | Handle OLED rendering and encoder input |
| Timer Task | Handle scheduled and periodic actions |
| Logger Task | Log data and system events |

Each task:
- Has a single responsibility
- Runs independently
- Communicates via queues or events

---

## Finite State Machines (FSM)

FSMs are used extensively to manage system behavior.

Key FSMs include:
- System Supervisor FSM
- Temperature Control FSM
- Humidity Control FSM
- Water Level FSM
- Alarm FSM
- UI FSM

FSMs provide:
- Explicit state representation
- Predictable transitions
- Simplified safety handling
- Easier testing and debugging

---

## Hardware Abstraction Layers (HAL)

### Sensor HAL
- Abstracts temperature, humidity, and water level sensors
- Provides unified sensor data and health status
- Isolates sensor-specific drivers from control logic

### Actuator HAL
- Abstracts relay-based outputs
- Provides a common ON/OFF control interface
- Ensures default safe states

HAL layers allow hardware changes without modifying control FSMs.

---

## Profile Configuration System

Profiles define how the software behaves for different applications.

A profile specifies:
- Enabled sensors
- Enabled actuators
- Default setpoints
- Safety limits
- Timers and schedules

Profiles allow the same firmware to support:
- Thermostat
- Incubator
- Cooler

No control logic is rewritten when switching profiles.

---

## Inter-Task Communication

The software uses:
- **Queues** for data and commands
- **Event Groups** for alarms and fault signaling
- **Shared read-only system status** for UI updates

This approach:
- Avoids race conditions
- Maintains data ownership
- Keeps tasks loosely coupled

---

## Safety and Fault Handling

Safety is enforced at multiple levels:
- Sensor health validation
- FSM-based fault states
- Alarm prioritization
- Safe shutdown of actuators

No actuator action bypasses safety logic.

---

## Logging and Diagnostics

The software includes:
- Periodic system logging
- Event-based logging
- Alarm logging

Initial logging is performed via serial output, with future support for SD card and cloud logging.

---

## Scalability and Maintainability

The software architecture supports:
- Adding new sensors and actuators
- Adding new profiles
- Expanding the UI
- Integrating communication interfaces (Wi-Fi, MQTT)

All expansions follow existing interfaces and architectural rules.

---

## Summary

The software architecture:
- Is structured and deterministic
- Aligns with RTOS best practices
- Supports long-term reuse
- Prioritizes safety and clarity

This software foundation enables reliable environmental control across multiple products.

---

➡️ Next: **Software → RTOS Architecture**
