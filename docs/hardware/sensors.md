# Sensors

!!! note "Design-phase document"
    Written before implementation (Phase 0). Most concepts still apply, but
    some details differ in the as-built firmware (v2.0.0): input is three push
    buttons (no rotary encoder), there is no water-level sensor, control logic
    is task-based rather than formal FSMs, and Wi-Fi + cloud telemetry are
    implemented. Pages outside the *Design Archive* section reflect the
    current firmware.


## Introduction

This document describes the **sensor subsystem** used in the  
**Reusable Environmental Control Platform**.

The sensor design focuses on:
- Reliability
- Accuracy suitable for control systems
- Electrical simplicity
- Long-term maintainability

All sensors are accessed through a **Sensor Hardware Abstraction Layer (HAL)** to ensure reusability and clean separation from control logic.

---

## Sensor Overview

| Parameter | Sensor | Interface |
|--------|--------|----------|
| Temperature | DS18B20 | 1-Wire |
| Humidity | DHT22 (AM2302) | Digital |
| Water Level | Float / Analog Sensor | Digital / ADC |

---

## Temperature Sensor – DS18B20

### Description
The DS18B20 is a **digital temperature sensor** using the 1-Wire protocol.

It is selected due to:
- Good accuracy and stability
- Noise immunity
- Support for long cable lengths
- Simple digital interface

### Electrical Connection
- DATA → GPIO 4
- Pull-up resistor: **4.7 kΩ** to 3.3 V (mandatory)
- Power: 3.3 V (recommended, non-parasitic mode)

### Characteristics
- Resolution: 9–12 bit (configurable)
- Typical accuracy: ±0.5 °C
- Conversion time: up to ~750 ms (12-bit)

### Software Handling
- Read periodically (1 Hz typical)
- Invalid readings (e.g., -127 °C) treated as sensor failure
- Sensor health reported to control logic

---

## Humidity Sensor – DHT22

### Description
The DHT22 (AM2302) is a **digital humidity and temperature sensor**.

It is selected because:
- Adequate accuracy for incubator and cooler applications
- Simple single-wire interface
- Wide availability and library support

### Electrical Connection
- DATA → GPIO 16
- Power: 3.3 V
- Pull-up resistor typically included on module

### Characteristics
- Humidity range: 0–100 %
- Accuracy: ±2–5 % RH
- Minimum read interval: **2 seconds**

### Software Handling
- Read at 2-second intervals or slower
- Last valid value cached
- Communication errors flagged as sensor warning or fault depending on profile

---

## Water Level Sensor

### Description
The water level sensor is used to monitor water availability for:
- Humidification
- Cooling systems
- Safety protection

The platform supports:
- Digital float switches
- Analog level sensors

### Electrical Connection
- Signal → GPIO 34 (input-only)
- Power depends on sensor type

### Software Handling
- Continuous or periodic monitoring
- Low-level detection triggers alarms
- Used by Water Level FSM

---

## Sensor Health Monitoring

Each sensor reports both:
- **Measured value**
- **Health status**

Health status allows the system to:
- Disable affected control loops
- Enter safe states
- Trigger alarms

Example conditions:
- Disconnected DS18B20
- DHT22 timeout or invalid data
- Water sensor open/short

---

## Sensor Abstraction Strategy

Sensors are accessed only through the **Sensor HAL**, which:
- Hides sensor-specific code
- Provides a unified data structure
- Simplifies testing using simulated drivers

Control FSMs never interact with raw sensor hardware.

---

## Profile-Based Sensor Usage

Not all profiles use all sensors:

| Profile | Temp | Humidity | Water Level |
|------|------|---------|------------|
| Thermostat | ✔ | ✖ | ✖ |
| Incubator | ✔ | ✔ | ✔ |
| Cooler | ✔ | ✖ | ✔ |

Unused sensors may be ignored or powered down to reduce overhead.

---

## Safety Considerations

- Sensor failures always override normal operation
- Invalid data never drives actuators
- Sensor faults are logged and displayed to the user

---

## Summary

The sensor subsystem:
- Provides reliable environmental data
- Is fully abstracted from control logic
- Supports multiple profiles
- Enables safe and deterministic control

This design ensures the platform remains robust and extensible.

---

➡️ Next: **Hardware → Actuators**
