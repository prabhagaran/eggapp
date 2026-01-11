# Design Philosophy

## Introduction

The design philosophy of the **Reusable Environmental Control Platform** is based on the idea that **good embedded systems are built as systems first, and code second**.

Every architectural decision in this project is driven by:
- Long-term maintainability
- Reusability across products
- Deterministic behavior
- Safety and clarity

This philosophy ensures the platform remains reliable and extensible as it grows.

---

## Systems Over Events

Instead of reacting to isolated events, this platform is designed as a **continuous system**.

Traditional approach:
- Button pressed → do something
- Sensor changed → react immediately

This platform’s approach:
- Sensors continuously measure
- The system continuously evaluates state
- Actions are taken based on **system state**, not single events

This results in:
- Predictable behavior
- Easier debugging
- Fewer edge-case failures

---

## FreeRTOS as the Execution Backbone

FreeRTOS is used not for complexity, but for **clarity and determinism**.

Reasons for choosing FreeRTOS:
- Independent tasks for independent responsibilities
- Predictable timing behavior
- Clean separation of sensor reading, control logic, UI, and actuation
- Scalable architecture as features grow

Each task has:
- A single responsibility
- Defined communication paths
- No hidden dependencies

This avoids the common “giant loop” problem seen in many embedded projects.

---

## Finite State Machines (FSM) for Control Logic

All control logic in the platform is implemented using **Finite State Machines (FSMs)**.

FSMs are used because they:
- Make behavior explicit
- Prevent undefined states
- Simplify safety handling
- Are easy to reason about and test

Examples:
- Temperature FSM (IDLE / HEATING / COOLING / FAULT)
- System FSM (INIT / AUTO / MANUAL / FAULT)
- Alarm FSM (CLEAR / ACTIVE / ACKED)

FSMs ensure the system always knows **what state it is in and why**.

---

## Hardware Abstraction as a Core Principle

The platform strictly separates:
- **What the system wants to do**
- **How the hardware actually does it**

This is achieved through:
- Sensor Hardware Abstraction Layer (HAL)
- Actuator Hardware Abstraction Layer (HAL)

Benefits:
- Hardware can change without touching logic
- Easy testing using simulated drivers
- Cleaner, more readable code

The control logic never accesses GPIOs directly.

---

## Profile-Driven Behavior

Rather than writing different code for each product, the platform uses **profiles**.

A profile defines:
- Which sensors are enabled
- Which actuators are active
- Control limits and defaults
- UI menu structure

Examples:
- Thermostat profile
- Incubator profile
- Cooler profile

This allows one firmware to behave like many products by changing configuration, not logic.

---

## Simple and Reliable User Interface

The user interface follows a **Nokia-style design philosophy**:
- Text-based
- Menu-driven
- No animations
- Predictable navigation

Reasons for this choice:
- High reliability
- Low processing overhead
- Works well with rotary encoder input
- Easy to use in industrial and home environments

The UI never directly controls hardware.  
It only sends **intent** to the control system.

---

## Safety First, Always

Safety is treated as a **design constraint**, not a feature.

This includes:
- Sensor fault detection
- Alarm prioritization
- Safe shutdown of actuators
- User acknowledgment for critical faults

No control action bypasses safety rules.

If safety conditions are violated, the system always moves to a **known safe state**.

---

## Reusability Over Optimization

This platform prioritizes:
- Readability over clever tricks
- Clear structure over compact code
- Reusability over micro-optimizations

This ensures:
- Easier maintenance
- Faster onboarding for new developers
- Lower risk of regression bugs

---

## Summary

The design philosophy of this platform can be summarized as:

- Think in systems, not events
- Separate logic from hardware
- Use FSMs for clarity and safety
- Use RTOS for structure, not complexity
- Prefer reuse over rewriting
- Keep the user interface simple and robust

This philosophy guides every technical decision in the project.

---

➡️ Next: **Overview → System Architecture**
