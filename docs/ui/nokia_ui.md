# Nokia Style UI

## Introduction

This document defines the **Nokia-style User Interface philosophy** used in the  
**Reusable Environmental Control Platform**.

The UI is inspired by classic Nokia phones (e.g., 3310 / 1100), focusing on:
- Simplicity
- Predictability
- Reliability
- Low cognitive load

This style is particularly well-suited for embedded control systems.

---

## Why Nokia Style UI?

The Nokia-style UI is chosen because it:
- Works reliably on small displays
- Requires minimal processing power
- Is easy to learn and remember
- Performs well in industrial and home environments
- Scales across multiple products

Unlike graphical or touch-based UIs, it avoids complexity and ambiguity.

---

## Core Nokia UI Principles

The following principles are strictly followed:

1. **One screen, one purpose**
2. **Vertical list navigation**
3. **Explicit cursor (`>`) for selection**
4. **Single action per input**
5. **Always provide a way back**
6. **No animations or transitions**
7. **Consistent behavior everywhere**

These rules ensure predictable interaction.

---

## Input Mapping (Encoder → Nokia Keys)

The rotary encoder emulates classic Nokia keypad behavior.

| Nokia Action | Encoder Input |
|------------|---------------|
| Scroll Up | Rotate Counter-Clockwise |
| Scroll Down | Rotate Clockwise |
| Select | Short Press |
| Back | Long Press |

This mapping is consistent across all screens.

---

## UI Screen Types

The UI consists of a small set of reusable screen types.

---

### Boot Screen

Purpose:
- Show product identity
- Show firmware version
- Show active profile

Characteristics:
- Displayed briefly at startup
- Automatically transitions to Home screen

Example:
ENV CONTROL
FW v1.0
PROFILE: INCUBATOR


---

### Home Screen (Status Screen)

Purpose:
- Display current system status
- Provide read-only information

Characteristics:
- No cursor
- No editing
- Short press opens menu

Example:

TEMP: 37.5 C
HUM : 55 %
MODE: AUTO
STATE: HEATING


---

### Menu List Screen

Purpose:
- Navigate configuration options

Characteristics:
- Vertical list
- Cursor (`>`) indicates selection
- Profile-dependent menu items

Example:
Set Temp
Set Hum
Mode
Timers
Alarms
Back


Behavior:
- Rotate → move cursor
- Short press → enter
- Long press → go back

---

### Value Edit Screen

Purpose:
- Edit a single parameter safely

Characteristics:
- One value displayed at a time
- Clear context (parameter name)
- Preview before confirmation

Example:
SET TEMP
[ 37.5 C ]


Behavior:
- Rotate → adjust value
- Short press → save
- Long press → cancel

---

### Alarm Screen

Purpose:
- Alert the user to abnormal conditions

Characteristics:
- Blocks all other UI interaction
- Clear alarm message
- Requires acknowledgment

Example:

!!! ALARM !!!
WATER LEVEL LOW
Press to ACK


Behavior:
- Short press → acknowledge
- Long press → ignored

---

## Menu Structure Rules

- Menu depth is kept shallow (max 2 levels)
- Menu order is consistent across profiles
- Irrelevant items are hidden, not disabled
- “Back” option is always present where applicable

---

## Profile-Based UI Adaptation

The UI dynamically adapts to the active profile.

Examples:

### Thermostat Profile

Set Temp
Mode
Alarms
Back


### Incubator Profile
Set Temp
Set Hum
Water Level
Rotation Timer
Alarms
Back


### Cooler Profile

Set Temp
Water Level
Fan Status
Alarms
Back


This keeps the UI clean and relevant.

---

## Error Prevention and Safety

The Nokia-style UI helps prevent errors by:
- Limiting editable values
- Enforcing one action per screen
- Requiring explicit confirmation
- Blocking changes during alarms

The UI never bypasses safety logic.

---

## Performance Characteristics

- Text-only rendering
- Low refresh rate (5–10 Hz)
- Minimal memory usage
- No dynamic allocation

These characteristics ensure smooth operation under RTOS.

---

## Summary

The Nokia-style UI provides:
- Predictable interaction
- Minimal complexity
- High reliability
- Excellent suitability for embedded control systems

This UI design supports long-term usability and system safety.

---

➡️ Next: **User Interface → Encoder Mapping**
