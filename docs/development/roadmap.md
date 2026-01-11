# Roadmap

## Introduction

This document outlines the **development roadmap** for the  
**Reusable Environmental Control Platform**.

The roadmap defines how the platform evolves from a **Thermostat MVP** into a **fully featured, multi-product environmental control system**.

Each phase builds on a stable foundation and avoids architectural rewrites.

---

## Roadmap Principles

The roadmap follows these guiding principles:

- Validate before expanding
- Add features incrementally
- Maintain backward compatibility
- Avoid premature optimization
- Preserve core architecture

No phase should compromise system stability.

---

## Phase 0 – Documentation & Architecture (Completed)

Status: ✔ Completed

Goals:
- Define system vision and philosophy
- Finalize hardware pinout
- Design software architecture
- Define FSMs and profiles
- Establish UI style

Outcome:
- Architecture frozen
- Documentation finalized
- Ready for implementation

---

## Phase 1 – Thermostat MVP

Status: 🔄 In Progress

Goals:
- Implement FreeRTOS task structure
- Implement Sensor and Actuator HAL
- Implement Temperature Control FSM
- Implement Alarm system
- Implement Nokia-style UI
- Validate safety behavior

Outcome:
- Fully functional thermostat
- Stable base for expansion

---

## Phase 2 – Incubator Profile

Status: ⏳ Planned

Goals:
- Enable humidity sensing and control
- Enable water level monitoring
- Implement rotation motor control
- Add incubator-specific UI menus
- Tune control parameters

Outcome:
- Fully functional incubator profile
- No core logic changes

---

## Phase 3 – Cooler Profile

Status: ⏳ Planned

Goals:
- Enable cooling-only control
- Integrate pump-based cooling
- Implement water safety logic
- Optimize UI for cooler operation

Outcome:
- Cooler profile enabled
- Same firmware, new behavior

---

## Phase 4 – Advanced Features

Status: ⏳ Planned

Potential features:
- Timer-based automation
- Data logging to SD card
- Configuration persistence (EEPROM / Flash)
- Profile scheduling

Outcome:
- Improved usability and automation

---

## Phase 5 – Connectivity & Cloud Integration

Status: ⏳ Optional

Potential features:
- Wi-Fi configuration
- MQTT or REST API
- Remote monitoring
- OTA firmware updates

Outcome:
- Connected environmental control platform

---

## Phase 6 – Productization

Status: ⏳ Optional

Goals:
- PCB optimization
- Enclosure design
- Certification considerations
- Manufacturing readiness

Outcome:
- Commercial-grade product

---

## Continuous Improvement

Across all phases:
- Bugs are fixed without breaking architecture
- Documentation is kept in sync
- Refactoring is allowed only if it improves clarity
- Safety remains the top priority

---

## Summary

This roadmap provides:
- A clear path from MVP to full platform
- Controlled and safe expansion
- Long-term sustainability
- Alignment with original vision

The roadmap ensures the platform grows **by design, not by accident**.

---

➡️ Next: **Appendix → Glossary**
