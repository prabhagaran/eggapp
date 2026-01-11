# Development Flow

## Introduction

This document defines the **recommended development and integration flow** for the  
**Reusable Environmental Control Platform**.

The purpose of this flow is to:
- Reduce integration risk
- Isolate faults early
- Maintain architectural integrity
- Ensure reliable and repeatable development

The project follows a **documentation-first, layer-by-layer approach**.

---

## Core Development Principles

The following principles must be followed throughout development:

1. **Documentation before implementation**
2. **One layer at a time**
3. **Test before integrating**
4. **No shortcuts around safety**
5. **Profiles define behavior, not code changes**

Violating these principles increases complexity and risk.

---

## Development Phases

### Phase 1 – Documentation Freeze

Goals:
- Finalize requirements
- Lock architecture
- Define interfaces
- Establish pinout and profiles

Outcome:
- Stable reference documentation
- No ambiguous behavior

This phase must be completed before coding begins.

---

### Phase 2 – Project Skeleton Setup

Goals:
- Create Arduino project structure
- Set up file organization
- Create empty module files
- Configure FreeRTOS basics

Outcome:
- Clean, buildable project with no functionality

---

### Phase 3 – Hardware Abstraction Layer (HAL)

Goals:
- Implement Sensor HAL
- Implement Actuator HAL
- Verify GPIO mapping
- Test hardware independently

Tests:
- Read sensors via serial output
- Toggle relays manually

No control logic is involved at this stage.

---

### Phase 4 – Control FSM Implementation

Goals:
- Implement System FSM
- Implement Temperature FSM
- Implement Humidity FSM
- Implement Water Level FSM

Tests:
- Inject simulated sensor data
- Verify FSM state transitions
- Confirm correct actuator commands

UI is not involved at this stage.

---

### Phase 5 – Profile Integration

Goals:
- Implement profile selection
- Enable/disable FSMs based on profile
- Load default setpoints and limits

Tests:
- Switch profiles
- Verify behavior changes without code modification

---

### Phase 6 – UI Integration

Goals:
- Implement Nokia-style UI FSM
- Integrate rotary encoder
- Display system status
- Send UI commands to control logic

Tests:
- Navigate menus
- Edit values safely
- Verify UI does not affect control timing

---

### Phase 7 – Alarm & Logging Integration

Goals:
- Implement Alarm FSM
- Integrate alarm UI
- Implement Logger Task

Tests:
- Force alarm conditions
- Verify safe-state behavior
- Confirm log output

---

### Phase 8 – System Integration Testing

Goals:
- Verify full system operation
- Test all profiles
- Validate safety behavior

Tests:
- Power cycling
- Sensor failure simulation
- Long-duration stability tests

---

## Incremental Testing Strategy

At every phase:
- Test only newly added functionality
- Do not modify unrelated modules
- Use serial logs for validation
- Keep changes small and reversible

This prevents cascading failures.

---

## Version Control Strategy

Recommended practices:
- Use Git for version control
- Commit after each phase
- Tag stable milestones
- Keep documentation and code in sync

Documentation changes should be committed alongside code changes.

---

## MVP Definition

The first functional milestone is the **Thermostat Profile MVP**.

Includes:
- Temperature control
- Heating and cooling
- OLED UI
- Encoder input
- Alarms and logging

All other features build upon this MVP.

---

## Common Mistakes to Avoid

- Mixing hardware logic with control FSMs
- Skipping HAL abstraction
- Adding UI logic into control tasks
- Ignoring fault handling
- Changing architecture mid-implementation

---

## Summary

The development flow:
- Enforces discipline and clarity
- Minimizes integration risk
- Preserves system architecture
- Supports long-term scalability

Following this flow ensures a robust and maintainable platform.

---

➡️ Next: **Development → MVP Definition**
