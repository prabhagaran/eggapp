# Hardware Overview

## Introduction

This document provides an overview of the **hardware architecture** used in the  
**Reusable Environmental Control Platform**.

The hardware is designed to be:
- Modular
- Electrically safe
- Scalable
- Reusable across multiple products

All hardware decisions support the software architecture and long-term reuse goals.

---

## Core Hardware Components

The platform consists of the following main hardware blocks:

ESP32 Controller
│
├── Sensors
│ ├── Temperature Sensor (DS18B20)
│ ├── Humidity Sensor (DHT22)
│ └── Water Level Sensor
│
├── Actuators (via Relay Board)
│ ├── Heating Element
│ ├── Cooling Fan
│ ├── Humidifier
│ ├── Water Pump
│ └── Rotation Motor
│
└── User Interface
├── OLED Display (I²C)
└── Rotary Encoder


---

## Microcontroller Unit (MCU)

### ESP32

The ESP32 is selected as the main controller due to:
- Dual-core architecture
- Built-in FreeRTOS support
- Sufficient GPIO availability
- Integrated peripherals (I²C, SPI, UART)
- Strong community and library support

Key characteristics:
- 32-bit Xtensa CPU
- 3.3 V logic level
- Multiple hardware timers
- Interrupt-capable GPIOs

---

## Sensor Subsystem

### Temperature Sensor – DS18B20
- Digital 1-Wire sensor
- High accuracy and stability
- Suitable for long cable runs
- Resistant to electrical noise

### Humidity Sensor – DHT22
- Digital humidity and temperature sensor
- Adequate accuracy for incubator and cooler applications
- Simple single-wire interface

### Water Level Sensor
- Float switch or analog level sensor
- Used for monitoring water availability
- Critical for incubator and cooler profiles

All sensors are electrically isolated from control logic through the **Sensor HAL**.

---

## Actuator Subsystem

### Relay-Based Outputs

All high-power devices are controlled using relays.

Reasons for using relays:
- Electrical isolation
- Ability to switch AC and DC loads
- Safety and reliability

Relay control is handled through:
- ULN2803 or opto-isolated relay boards
- Flyback protection for inductive loads

Supported actuators:
- Heater
- Cooling fan
- Humidifier
- Water pump
- Rotation motor

The system supports **up to 8 relays**, allowing future expansion.

---

## User Interface Hardware

### OLED Display
- I²C interface
- Text-based display
- Low power consumption
- High contrast and readability

The OLED is used for:
- Status display
- Menu navigation
- Alarm indication

---

### Rotary Encoder
- Quadrature rotary encoder with push button
- Used for menu navigation and value adjustment
- Provides precise and reliable user input

The encoder is connected to interrupt-capable GPIOs for responsiveness.

---

## Power Architecture

### Logic Power
- ESP32 and sensors operate at **3.3 V**
- Regulated supply required

### Relay Power
- Relays typically operate at **5 V or 12 V**
- Relay supply is electrically isolated from ESP32 logic

### Safety Considerations
- Common ground between logic and driver (if required)
- Proper isolation for AC loads
- Adequate current rating for relays

---

## Hardware Design Principles

The hardware design follows these principles:
- Electrical isolation between high-power and logic domains
- Clear pin assignments
- Expandability without PCB redesign
- Compatibility with multiple profiles

---

## Summary

The hardware architecture provides:
- A reliable base for environmental control
- Electrical safety and isolation
- Flexibility for future expansion
- Strong alignment with the software architecture

This hardware foundation supports all current and future profiles of the platform.

---

➡️ Next: **Hardware → Pinout**
