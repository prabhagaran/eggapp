# System Architecture

!!! note "Design-phase document"
    Written before implementation (Phase 0). Most concepts still apply, but
    some details differ in the as-built firmware (v2.0.0): input is three push
    buttons (no rotary encoder), there is no water-level sensor, control logic
    is task-based rather than formal FSMs, and Wi-Fi + cloud telemetry are
    implemented. Pages outside the *Design Archive* section reflect the
    current firmware.


## Introduction

This document describes the **overall system architecture** of the  
**Reusable Environmental Control Platform**.

The architecture is designed to be:
- Modular
- Deterministic
- Scalable
- Reusable across multiple products

Every subsystem has a **clear responsibility** and communicates through **well-defined interfaces**.

---

## High-Level Architectural View

At the highest level, the platform is a **closed-loop environmental control system**.

Sensors → Sensor HAL → Control FSM → Actuator HAL → Outputs
↑
Profiles
↑
UI FSM ← Encoder + OLED


This structure ensures:
- Control logic is isolated from hardware
- UI is isolated from control logic
- Profiles define behavior, not code

---

## Layered Architecture

The system is divided into **logical layers**, each with a specific role.

┌──────────────────────────────────────┐
│ User Interface Layer (OLED + Encoder)│
├──────────────────────────────────────┤
│ UI FSM & Commands │
├──────────────────────────────────────┤
│ System & Control FSMs │
├──────────────────────────────────────┤
│ Profile Configuration Layer │
├──────────────────────────────────────┤
│ Sensor HAL | Actuator HAL │
├──────────────────────────────────────┤
│ FreeRTOS (Tasks, Queues, Events) │
├──────────────────────────────────────┤
│ ESP32 Hardware │
└──────────────────────────────────────┘

Each layer depends only on the layer directly below it.

---

## Core Architectural Components

### Sensor Subsystem

- Responsible for acquiring raw environmental data
- Uses sensor-specific drivers internally
- Exposes a unified data structure to the system

Functions:
- Temperature measurement (DS18B20)
- Humidity measurement (DHT22)
- Water level measurement

The control logic never accesses sensor hardware directly.

---

### Control Subsystem (FSM-Based)

The control subsystem is the **brain of the platform**.

It consists of multiple Finite State Machines:
- System Supervisor FSM
- Temperature Control FSM
- Humidity Control FSM
- Water Level FSM
- Alarm FSM

Each FSM:
- Operates independently
- Has well-defined states
- Communicates using shared system state and events

This ensures predictable behavior under all conditions.

---

### Profile Configuration Layer

Profiles define **how the system behaves** without changing code.

A profile specifies:
- Enabled sensors
- Enabled actuators
- Default setpoints
- Safety limits
- Timers and schedules

By switching profiles, the same firmware can act as:
- Thermostat
- Incubator
- Cooler

---

### Actuator Subsystem

The actuator subsystem translates **control intent** into physical action.

Characteristics:
- Fully hardware-abstracted
- Relay-based outputs
- Mutual exclusion enforced at logic level

Controlled devices include:
- Heating element
- Cooling fan
- Humidifier
- Water pump
- Rotation motor

Actuators never make decisions; they only execute commands.

---

### User Interface Subsystem

The UI subsystem provides local interaction via:
- OLED display
- Rotary encoder

Responsibilities:
- Display system status
- Provide menu-driven configuration
- Send user intent to control subsystem
- Handle alarm acknowledgment

The UI never directly controls actuators.

---

## RTOS Task Architecture

FreeRTOS is used to run each major subsystem independently.

| Task | Responsibility |
|----|----|
| Sensor Task | Read sensors & publish data |
| Control Task | Execute FSM logic |
| Actuator Task | Drive outputs |
| UI Task | Handle OLED & encoder |
| Timer Task | Scheduled actions |
| Logger Task | Logging & diagnostics |

This ensures:
- Non-blocking execution
- Predictable timing
- Clear ownership of responsibilities

---

## Data Flow & Communication

Communication between tasks is done using:
- Queues for data and commands
- Event groups for alarms and faults
- Shared read-only system status for UI

This avoids:
- Global variable misuse
- Race conditions
- Tight coupling between modules

---

## Safety Architecture

Safety is embedded at every layer:
- Sensor health monitoring
- FSM-based fault states
- Alarm escalation
- Safe-state enforcement for actuators

Any critical fault causes the system to:
1. Disable actuators
2. Raise alarms
3. Await user acknowledgment

---

## Scalability & Extension

The architecture supports future expansion without redesign:
- New sensors via HAL
- New actuators via HAL
- New profiles via configuration
- New UI features without affecting control logic
- Cloud and IoT integration as optional layers

---

## Summary

The system architecture ensures:
- Clear separation of concerns
- Deterministic and safe operation
- Long-term reusability
- Easy maintenance and expansion

This architecture forms the foundation for all implementations and future enhancements of the platform.

---

➡️ Next: **Hardware → Hardware Overview**
