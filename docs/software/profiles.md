# Profiles

## Introduction

This document describes the **Profile Configuration System** used in the  
**Reusable Environmental Control Platform**.

Profiles are the key mechanism that allow **one firmware** to behave as **multiple products** without changing control logic or hardware abstraction layers.

---

## What Is a Profile?

A **profile** is a configuration that defines:
- What sensors are used
- What actuators are enabled
- What control FSMs are active
- Default setpoints and limits
- Timers and alarms

Profiles contain **data only**, not logic.

> Changing the profile changes system behavior  
> without rewriting or recompiling core logic.

---

## Why Profiles Are Used

Without profiles:
- Each product requires separate firmware
- Logic gets duplicated
- Maintenance becomes difficult
- Bug fixes must be repeated

With profiles:
- One codebase supports many products
- Behavior is data-driven
- Testing effort is reduced
- Long-term maintenance is simplified

---

## Profile Responsibilities

A profile is responsible for defining:

- Enabled sensors (temperature, humidity, water level)
- Enabled actuators (heater, fan, pump, etc.)
- Control limits and hysteresis values
- Default operating setpoints
- Timer behavior
- Alarm severity mapping

A profile is **not responsible** for:
- Control algorithms
- Hardware access
- UI rendering logic

---

## Profile Capability Definition

Profiles declare their capabilities explicitly.

Example capability flags:
- Use temperature control
- Use humidity control
- Use water level monitoring
- Enable specific actuators
- Enable timers

These flags are used by:
- Control FSMs (to enable/disable logic)
- UI (to show or hide menu items)
- Sensor task (to ignore unused sensors)

---

## Supported Profiles

The platform currently supports the following profiles:

### Thermostat Profile
Designed for basic temperature control.

Characteristics:
- Temperature control only
- Heating and cooling support
- No humidity or water level control
- Simple UI menu

Use cases:
- Room thermostat
- Heater controller
- Temperature chamber

---

### Incubator Profile
Designed for egg incubation and similar environments.

Characteristics:
- Temperature control
- Humidity control
- Water level monitoring
- Rotation motor control
- Timers and alarms enabled

Use cases:
- Egg incubator
- Controlled biological environments

---

### Cooler Profile
Designed for evaporative or water-assisted cooling systems.

Characteristics:
- Temperature control (cooling only)
- Water level monitoring
- Fan and pump control
- No humidity regulation

Use cases:
- Air cooler
- Evaporative cooling systems

---

## Profile-to-FSM Mapping

Profiles determine which FSMs are active.

| Profile | Temp FSM | Hum FSM | Water FSM | Timer FSM |
|------|---------|--------|----------|-----------|
| Thermostat | ✔ | ✖ | ✖ | ✖ |
| Incubator | ✔ | ✔ | ✔ | ✔ |
| Cooler | ✔ | ✖ | ✔ | ✖ |

FSM implementations remain unchanged across profiles.

---

## Profile-to-Actuator Mapping

Profiles also define which actuators are valid.

| Profile | Heater | Cooler | Humidifier | Pump | Rotation |
|------|-------|--------|------------|------|----------|
| Thermostat | ✔ | ✔ | ✖ | ✖ | ✖ |
| Incubator | ✔ | ✔ | ✔ | ✔ | ✔ |
| Cooler | ✖ | ✔ | ✖ | ✔ | ✖ |

Commands for disabled actuators are ignored.

---

## Default Setpoints and Limits

Each profile defines:
- Default temperature setpoint
- Minimum and maximum allowed values
- Hysteresis bands
- Default humidity (if applicable)
- Timer intervals (if applicable)

These defaults are loaded at startup or when switching profiles.

---

## Profile Selection

Profiles can be selected:
- At startup (default)
- Via the UI menu
- Via serial command (development)

Profile switching:
- Safely disables all actuators
- Reloads configuration values
- Reinitializes FSM states

---

## UI Integration

The UI adapts dynamically based on the active profile:
- Menu items appear only if relevant
- Values are editable only if enabled
- Status screens show only active parameters

This ensures a clean and user-friendly interface.

---

## Safety Considerations

Profiles cannot override safety rules.

Regardless of profile:
- Sensor failures force safe states
- Alarms override user actions
- Actuators are disabled during FAULT states

Safety logic always has higher priority than profile behavior.

---

## Extending Profiles

To add a new product:
1. Define a new profile configuration
2. Specify enabled sensors and actuators
3. Set limits and defaults
4. Update UI menu mapping if needed

No FSM or HAL changes are required.

---

## Summary

The profile system:
- Enables true firmware reuse
- Keeps logic clean and centralized
- Simplifies product variation
- Supports long-term scalability

Profiles are the foundation that transform this platform into a **multi-product system**.

---

➡️ Next: **User Interface → UI Overview**
