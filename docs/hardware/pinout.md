# Pinout

## Introduction

This document defines the **final and locked GPIO pin assignment** for the  
**Reusable Environmental Control Platform**.

The pinout is designed to:
- Avoid ESP32 boot and flash conflicts
- Support FreeRTOS operation
- Allow future expansion
- Remain common across all profiles

Once finalized, this pinout should not be changed without strong justification.

---

## ESP32 Pin Usage Strategy

The following ESP32 pin rules are applied:

- GPIO 6–11 → **Not used** (internal flash)
- Boot strapping pins avoided where possible
- Input-only pins used for sensors where applicable
- Interrupt-capable pins reserved for encoder inputs
- I²C pins fixed for OLED and future expansion

---

## Sensor Pin Assignment

| Sensor | GPIO | Notes |
|------|------|------|
| DS18B20 (Temperature) | GPIO 4 | 1-Wire, 4.7 kΩ pull-up to 3.3 V |
| DHT22 (Humidity) | GPIO 16 | Digital input |
| Water Level Sensor | GPIO 34 | Input-only, ideal for level sensing |

---

## User Interface Pin Assignment

### OLED Display (I²C)

| Signal | GPIO |
|------|------|
| SDA | GPIO 21 |
| SCL | GPIO 22 |

---

### Rotary Encoder

| Signal | GPIO | Notes |
|------|------|------|
| Encoder CLK (A) | GPIO 18 | Interrupt-capable |
| Encoder DT (B) | GPIO 19 | Interrupt-capable |
| Encoder SW | GPIO 23 | Push-button input |

---

## Actuator (Relay) Pin Assignment

The system supports **up to 8 relay outputs**.

| Relay | Function | GPIO |
|------|--------|------|
| Relay 1 | Heater | GPIO 25 |
| Relay 2 | Cooling Fan | GPIO 26 |
| Relay 3 | Humidifier | GPIO 27 |
| Relay 4 | Water Pump | GPIO 14 |
| Relay 5 | Rotation Motor | GPIO 12 |
| Relay 6 | Spare | GPIO 13 |
| Relay 7 | Spare | GPIO 32 |
| Relay 8 | Spare | GPIO 33 |

All relays are driven via a **ULN2803 or opto-isolated relay board**.

---

## Reserved / Unused Pins

| GPIO | Status |
|----|------|
| GPIO 0 | Avoided (boot mode) |
| GPIO 2 | Avoided (boot strapping) |
| GPIO 5 | Reserved |
| GPIO 15 | Avoided (boot strapping) |
| GPIO 6–11 | Not available (flash) |

---

## Electrical Notes

- All ESP32 GPIOs operate at **3.3 V logic**
- Do **not** connect relay coils directly to ESP32 pins
- Use proper isolation for AC loads
- Ensure common ground where required by driver circuits

---

## Expandability

The chosen pinout allows:
- Additional sensors via I²C
- Additional actuators via spare relays
- Future communication modules (Wi-Fi, UART, SPI)

---

## Summary

This pinout:
- Is safe and boot-reliable
- Supports all required features
- Allows future expansion
- Is common across all profiles

This document serves as the **single source of truth** for hardware connections.

---

➡️ Next: **Hardware → Sensors**
