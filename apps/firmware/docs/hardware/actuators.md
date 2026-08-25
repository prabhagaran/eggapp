# Actuators

!!! note "Design-phase document"
    Written before implementation (Phase 0). Most concepts still apply, but
    some details differ in the as-built firmware (v2.0.0): input is three push
    buttons (no rotary encoder), there is no water-level sensor, control logic
    is task-based rather than formal FSMs, and Wi-Fi + cloud telemetry are
    implemented. Pages outside the *Design Archive* section reflect the
    current firmware.


## Introduction

This document describes the **actuator subsystem** of the  
**Reusable Environmental Control Platform**.

Actuators are responsible for **physically modifying the environment** based on decisions made by the control FSMs.

The actuator design emphasizes:
- Electrical safety
- Hardware abstraction
- Deterministic behavior
- Reusability across profiles

---

## Actuator Overview

The platform controls the following actuators:

| Actuator | Function |
|--------|---------|
| Heating Element | Increase temperature |
| Cooling Fan | Reduce temperature |
| Humidifier | Increase humidity |
| Water Pump | Maintain water level |
| Rotation Motor | Periodic rotation (incubator) |

All actuators are driven through **relay outputs** and controlled via the **Actuator Hardware Abstraction Layer (HAL)**.

---

## Relay-Based Control

### Why Relays?

Relays are used because they provide:
- Electrical isolation between logic and load
- Ability to switch AC and DC loads
- High reliability for inductive and resistive loads
- Simple and well-understood behavior

Relays are suitable for:
- Heaters
- Fans
- Pumps
- Motors
- Humidifiers

---

## Relay Driver Interface

Relays are not driven directly by the ESP32.

Instead, the system uses:
- **ULN2803** transistor array  
  or  
- **Opto-isolated relay modules**

This provides:
- Flyback protection
- Proper current handling
- Reduced electrical noise

---

## Actuator Pin Assignment

As-built, from `egg_incubator_v2/config.h`. The design-phase 8-relay bank was
superseded during implementation; [pinout.md](pinout.md) is the authoritative
page and carries the strapping-pin and polarity notes.

| Actuator | GPIO | ON level |
|----------|------|----------|
| Heater | GPIO 26 | LOW |
| Cooler | GPIO 27 | LOW |
| Humidifier | GPIO 14 | **HIGH** — active-HIGH module |
| Fan | GPIO 13 | LEDC PWM, inverted duty |
| Pump | GPIO 16 | LOW |
| Turner | GPIO 17 | LOW |

Six channels, not eight. The board is active-LOW throughout except the
humidifier; resolve levels with `relayLevel(pin, on)` rather than writing
`RELAY_ON`/`RELAY_OFF` to a pin directly.

---

## Actuator Abstraction Layer (HAL)

The Actuator HAL ensures that:
- Control logic never accesses GPIOs directly
- Hardware can be changed without modifying control logic
- Actuator behavior remains consistent across profiles

### Responsibilities of Actuator HAL
- Initialize relay GPIOs
- Apply ON/OFF commands
- Enforce default safe states
- Provide a single interface for all actuators

---

## Mutual Exclusion & Safety Rules

Certain actuators must never be active simultaneously.

### Critical Rules
- Heater and Cooler must never be ON at the same time
- Actuators must turn OFF during system FAULT state
- Safety overrides user commands

These rules are enforced by the **control FSM**, not the HAL.

---

## Profile-Based Actuator Usage

Not all profiles use all actuators:

| Profile | Heater | Cooler | Humidifier | Pump | Rotation |
|------|-------|--------|------------|------|----------|
| Thermostat | ✔ | ✔ | ✖ | ✖ | ✖ |
| Incubator | ✔ | ✔ | ✔ | ✔ | ✔ |
| Cooler | ✖ | ✔ | ✖ | ✔ | ✖ |

Unused actuators are disabled at the profile level.

---

## Electrical Safety Considerations

- Do not exceed relay current ratings
- Use proper isolation for AC loads
- Ensure correct grounding strategy
- Separate high-voltage and low-voltage traces on PCB
- Use snubber circuits if required for inductive loads

---

## Default Safe State

On:
- Power-up
- Reset
- Sensor failure
- System fault

All actuators are driven to a **known safe OFF state**.

This behavior is mandatory and enforced at system level.

---

## Expandability

The actuator system allows:
- Adding new actuators via spare relays
- Replacing relays with SSRs or MOSFETs
- PWM-based actuators (future extension)

All extensions must conform to the Actuator HAL interface.

---

## Summary

The actuator subsystem:
- Provides safe and reliable physical control
- Is fully abstracted from control logic
- Supports multiple profiles
- Enables future expansion without redesign

This completes the hardware documentation for actuators.

---

➡️ Next: **Software → Software Overview**
