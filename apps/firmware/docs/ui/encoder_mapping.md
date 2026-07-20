# Encoder Mapping

!!! warning "Design archive — superseded by the as-built firmware"
    This page was written during the design phase. The shipped firmware
    (v2.0.0) uses an SSD1306 128x64 OLED with **three push buttons**
    (UP / DOWN / OK) — there is no rotary encoder. See *User Interface →
    UI Overview* and *Button Mapping* for the as-built behavior.


## Introduction

This document defines how the **rotary encoder** is used as the primary input device for the  
**Reusable Environmental Control Platform**.

The encoder mapping is designed to:
- Be intuitive and consistent
- Follow Nokia-style UI behavior
- Work reliably under FreeRTOS
- Remain reusable across all profiles

---

## Encoder Hardware Overview

The platform uses a standard mechanical rotary encoder with:
- **Channel A (CLK)**
- **Channel B (DT)**
- **Push Button (SW)**

This encoder provides:
- Incremental rotation input
- Direction detection
- A single confirmation / back button

---

## Encoder Pin Assignment

| Encoder Signal | ESP32 GPIO |
|--------------|-----------|
| CLK (A) | GPIO 18 |
| DT (B) | GPIO 19 |
| SW (Button) | GPIO 23 |

All encoder inputs use internal or external pull-ups as required.

---

## Encoder Event Abstraction

The encoder does not directly control the UI or system logic.

Instead, it generates **high-level events** that are consumed by the UI FSM.

### Encoder Events

```text
ENC_EVENT_CW      (Clockwise rotation)
ENC_EVENT_CCW     (Counter-clockwise rotation)
ENC_EVENT_PRESS   (Short button press)
ENC_EVENT_LONG    (Long button press)

| Encoder Action | UI Meaning                        |
| -------------- | --------------------------------- |
| Rotate CW      | Move cursor down / Increase value |
| Rotate CCW     | Move cursor up / Decrease value   |
| Short Press    | Select / Confirm                  |
| Long Press     | Back / Cancel                     |

This mapping is consistent across all UI screens.

Encoder Behavior by UI State
Home Screen

Rotate: No action

Short press: Enter menu

Long press: No action

Menu List Screen

Rotate: Move selection cursor

Short press: Enter selected menu item

Long press: Go back to previous screen

Value Edit Screen

Rotate: Adjust value

Short press: Save value

Long press: Cancel edit and restore previous value

Alarm Screen

Rotate: Ignored

Short press: Acknowledge alarm

Long press: Ignored

Debouncing and Filtering

To ensure reliable operation:

Mechanical bounce is filtered in software

Rotation events are rate-limited

Button presses use time-based detection

Typical thresholds:

Short press: < 1 second

Long press: ≥ 1 second

Exact timing values are configurable.

RTOS Integration

Encoder handling follows these rules:

GPIO interrupts detect raw changes

Minimal logic inside ISR

Events queued to encoder/UI task

UI FSM processes events asynchronously

This ensures:

No blocking in interrupts

Deterministic UI behavior

RTOS-safe input handling

Safety Rules

Encoder input is ignored during critical system faults (except alarm acknowledgment)

Encoder events never directly control actuators

All encoder actions are validated by UI and control logic

Reusability and Extensibility

The encoder mapping is:

Independent of hardware implementation

Independent of profile

Independent of control logic

Future extensions may include:

Acceleration for fast rotation

Double-click detection

Additional encoder-based shortcuts

These can be added without changing existing mappings.

Summary

The rotary encoder mapping:

Provides a simple and reliable input method

Matches Nokia-style UI principles

Integrates cleanly with FreeRTOS

Supports safe and deterministic system control

This mapping ensures consistent user interaction across all platform profiles.


---

## ✅ Encoder Mapping Page Status

✔ Encoder behavior fully defined  
✔ UI interaction rules clarified  
✔ RTOS-safe handling documented  
✔ Matches hardware and UI design  

---

### 🔜 Next Page

👉 **Safety & Logging → Alarms** (`safety/alarms.md`)  

Say: **“Write alarms page”** and we’ll continue 📘

