# Future Expansion

!!! note "See the live roadmap"
    This design-phase wishlist is superseded by **FEATURE_ROADMAP.md** in the
    repository root (2026-06-11), which lists 30+ candidate features with
    effort ratings and integration notes for the as-built firmware.


## Introduction

This document outlines potential **future expansion paths** for the  
**Reusable Environmental Control Platform**.

All expansions described here are designed to:
- Reuse existing architecture
- Avoid breaking changes
- Preserve safety and determinism
- Require minimal refactoring

The platform is intentionally designed to grow.

---

## Additional Sensors

Future sensors can be added through the **Sensor HAL** without modifying control logic.

Examples:
- CO₂ sensors
- Air quality sensors
- Pressure sensors
- Light intensity sensors
- Additional temperature zones

New sensors require:
- HAL driver implementation
- Profile configuration update
- Optional UI integration

---

## Advanced Control Algorithms

The current system uses ON–OFF control with hysteresis.

Future enhancements may include:
- PID control for temperature
- Adaptive control based on historical data
- Multi-zone control strategies

Control algorithm upgrades must:
- Remain inside FSMs
- Respect safety constraints
- Be selectable per profile

---

## Expanded Actuator Support

Actuator expansion options include:
- Solid State Relays (SSR)
- MOSFET-based drivers
- PWM-controlled actuators
- Variable-speed fans and pumps

All new actuators must conform to the **Actuator HAL interface**.

---

## Enhanced User Interface

UI enhancements may include:
- Graphical icons
- Multi-language support
- Advanced configuration menus
- Context-sensitive help screens

Any enhancements must:
- Preserve Nokia-style simplicity
- Remain non-blocking
- Not interfere with control logic

---

## Data Storage and Persistence

Future storage features may include:
- Setpoint persistence (EEPROM / Flash)
- SD card logging
- Circular log buffers
- Configuration backups

Storage logic must:
- Be non-blocking
- Handle wear leveling
- Fail gracefully

---

## Connectivity and Cloud Integration

Optional connectivity features:
- Wi-Fi configuration
- MQTT integration
- REST APIs
- Mobile or web dashboards
- OTA firmware updates

Connectivity must:
- Be optional
- Run in separate tasks
- Never block control logic

---

## Multi-Device and Networked Systems

The platform may be extended to:
- Control multiple nodes
- Synchronize multiple controllers
- Support master–slave configurations

These extensions must:
- Preserve local safety
- Allow standalone operation
- Handle communication failures gracefully

---

## Security Considerations

Future security enhancements may include:
- Authentication for remote access
- Encrypted communication
- Secure boot and firmware validation

Security features must not compromise system availability.

---

## Productization Enhancements

For commercial deployment:
- PCB optimization
- Enclosure design
- Thermal and EMI testing
- Regulatory compliance
- Manufacturing test fixtures

These steps build upon the existing hardware and software foundation.

---

## Architectural Stability

All future expansions must respect the following constraints:
- Core FSM architecture remains unchanged
- HAL boundaries are preserved
- Profiles remain configuration-only
- Safety logic always has priority

Breaking these rules requires a major version revision.

---

## Summary

The platform is intentionally designed for:
- Long-term growth
- Feature expansion
- Product diversification

Future development should enhance the platform **without compromising its core architecture**.

---

➡️ Documentation Complete
