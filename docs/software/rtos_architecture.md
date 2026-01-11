# RTOS Architecture

## Introduction

This document describes how **FreeRTOS** is used in the  
**Reusable Environmental Control Platform** to achieve deterministic, modular, and scalable software execution.

FreeRTOS is used as a **structuring tool**, not merely as a scheduler.

---

## Why FreeRTOS?

FreeRTOS is chosen to address common embedded system challenges:
- Blocking code and timing conflicts
- Monolithic `loop()` logic
- Tight coupling between subsystems
- Poor scalability as features grow

Using FreeRTOS allows the system to:
- Run independent tasks concurrently
- Maintain predictable timing behavior
- Isolate failures and delays
- Scale without redesign

---

## RTOS Design Principles

The following principles guide the RTOS architecture:

1. **One responsibility per task**
2. **No blocking delays across tasks**
3. **Explicit communication via queues and events**
4. **No direct cross-task access to hardware**
5. **Control logic centralized in FSMs**

These rules ensure clarity and maintainability.

---

## Task Overview

The platform uses the following FreeRTOS tasks:

| Task | Responsibility |
|----|----|
| Sensor Task | Read and validate sensor data |
| Control Task | Execute system and control FSMs |
| Actuator Task | Apply commands to hardware |
| UI Task | Handle OLED display and encoder input |
| Timer Task | Handle scheduled actions |
| Logger Task | Log system data and events |

Each task runs independently with a defined priority.

---

## Task Priority Strategy

Task priorities are assigned based on system criticality:

| Task | Priority | Rationale |
|----|----|-----------|
| Control Task | High | Core decision-making |
| Sensor Task | Medium | Data acquisition |
| Actuator Task | Medium | Output execution |
| UI Task | Low | Human interaction |
| Timer Task | Low | Scheduled operations |
| Logger Task | Low | Non-critical logging |

This ensures safety-critical logic always executes on time.

---

## Task Timing Behavior

Typical task execution intervals:

| Task | Interval |
|----|---------|
| Sensor Task | 1–2 seconds |
| Control Task | 200–500 ms |
| UI Task | 100–200 ms |
| Timer Task | 1 second |
| Logger Task | 30–60 seconds |

Intervals are configurable and profile-dependent.

---

## Inter-Task Communication

FreeRTOS primitives are used as follows:

### Queues
Used for:
- Sensor data transfer
- UI commands
- Actuator commands

Queues provide:
- Thread safety
- Clear data ownership
- Decoupled task interaction

---

### Event Groups
Used for:
- Alarm signaling
- Fault conditions
- System-wide notifications

Event groups enable fast, broadcast-style communication.

---

### Shared System Status

A shared system status structure is:
- Written only by the Control Task
- Read-only for UI and Logger tasks
- Protected using a mutex

This avoids race conditions while providing real-time visibility.

---

## Core Pinning (ESP32 Specific)

ESP32 has two cores:
- Core 0: System and background tasks
- Core 1: Application tasks

Recommended pinning:
- Control Task → Core 1
- Sensor Task → Core 1
- Actuator Task → Core 0
- UI Task → Core 0

This separation improves responsiveness and stability.

---

## Fault Isolation

The RTOS architecture allows:
- A blocked or delayed task to be isolated
- Faults to be escalated via events
- Safe shutdown regardless of task state

Control logic never depends on UI or logging tasks.

---

## RTOS Startup Sequence

1. Hardware initialization
2. HAL initialization
3. Queue and event creation
4. Task creation
5. System enters INIT state
6. Normal operation begins

This sequence ensures consistent startup behavior.

---

## RTOS and Arduino Loop

The Arduino `loop()` function is intentionally left empty.

FreeRTOS tasks fully control system execution.

This prevents:
- Hidden execution paths
- Timing conflicts
- Accidental blocking

---

## Summary

The RTOS architecture:
- Structures system execution cleanly
- Enables deterministic behavior
- Supports long-term scalability
- Aligns with safety and reliability goals

FreeRTOS is a foundational component that enables the platform’s modular design.

---

➡️ Next: **Software → FSM Design**
