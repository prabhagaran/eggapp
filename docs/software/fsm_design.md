# FSM Design

## Purpose and scope

This document defines the Finite State Machine (FSM) approach used by the
Reusable Environmental Control Platform. It describes the FSM architecture,
interaction rules, implementation patterns, safety considerations, and test
recommendations. The intent is to make control behavior deterministic,
auditable, and easy to reason about during development and maintenance.

## Design principles

- Single responsibility per FSM: each FSM encapsulates one orthogonal concern
	(e.g., temperature control, humidity control, water management).
- Deterministic transitions: states and transitions are explicit and driven by
	well-defined events or timed conditions.
- Separation of concerns: FSMs make decisions and produce commands; a
	dedicated actuator layer executes commands.
- Fail-safe defaults: invalid inputs or faults must transition FSMs to safe
	states that disable risky actuators.

## FSM taxonomy and responsibilities

Top-level supervisor:
- System Supervisor FSM — manages global operating modes and coordinates
	subsystem enablement and fault handling.

Subsystems:
- Temperature Control FSM — maintains temperature using hysteresis, PWM or
	relay outputs.
- Humidity Control FSM — maintains humidity through humidifier/dehumidifier
	actions where applicable.
- Water Level FSM — monitors reservoir state and controls filling logic.
- Alarm FSM — centralizes alarm aggregation, escalation, and user
	acknowledgement.

Each FSM is intentionally scoped so logic remains small and testable.

## State model and events

Representation:
- Each FSM exposes an initialization function, a step/update function, and an
	event handler. A minimal API is:

- `fsm_init()` — configure initial state and timers
- `fsm_handle_event(event_t e)` — apply external events
- `fsm_step()` — run periodic evaluation (called from Control Task loop)

Events:
- Sensor updates (e.g., temperature_reading)
- Profile or user commands (e.g., set_mode_auto)
- Timers and watchdog expirations
- Fault detections and health changes

Transitions must be guarded by clear preconditions and may optionally be
debounced by time windows to avoid oscillation.

## Example: Temperature Control FSM

States: `TEMP_IDLE`, `TEMP_HEATING`, `TEMP_COOLING`, `TEMP_FAULT`

Transition logic (pseudo):

- If sensor invalid → `TEMP_FAULT`
- If temperature < setpoint − hysteresis → enter `TEMP_HEATING`
- If temperature > setpoint + hysteresis → enter `TEMP_COOLING`
- If within band → `TEMP_IDLE`

Actuation: heating and cooling commands are issued as high-level requests
(e.g., `request_heater(on)`, `request_fan(speed)`) rather than direct GPIO
writes. The actuator layer serializes and arbitrates actual hardware access.

Hysteresis and timing
- Use conservative hysteresis values and minimum on/off durations to prevent
	rapid cycling of relays or compressors. Typical defaults are documented in
	`egg_incubator_v2/config.h`.

## Inter-FSM coordination and priority

Coordination mechanisms:
- Event groups or atomic flags for cross-FSM signals (e.g., `FAULT_ACTIVE`).
- Command queues for intent passing from FSMs to the actuator manager.
- Supervisor-enforced arbitration where the System Supervisor can disable
	subsystem outputs in fault or maintenance modes.

Priority rules:
- System Supervisor > Alarm FSM > Control FSMs > Actuator requests

Implementation guidance:
- No FSM should directly toggle hardware. Instead, produce a command object
	placed on a queue consumed by the Actuator Task.
- Use mutexes or atomic variables when multiple producers/readers access shared
	state (see `egg_incubator_v2/globals.h`).

## Execution context and timing

- All FSMs are evaluated from the Control Task context to maintain ordering
	and simplify timing assumptions.
- `fsm_step()` calls should be non-blocking and bounded in execution time.
- For periodic evaluation, use a fixed step interval (e.g., 250–500 ms) and
	avoid long-running computations inside FSM code.

## Safety and fault handling

Fault policy:
- Sensor validation failures immediately transition related FSMs to FAULT
	states and emit alarm events.
- The Alarm FSM aggregates alarms and instructs the System Supervisor to
	enforce fail-safe modes (disable heaters, set fan to safe speed, etc.).
- Recovery from critical faults requires explicit user acknowledgement; soft
	faults (e.g., transient sensor glitch) may allow automatic recovery.

Watchdog and HW safety:
- Initialize the task watchdog in `egg_incubator_v2/egg_incubator_v2.ino` and
	ensure tasks that can block are either excluded from watchdog checks or
	periodically pet the watchdog as appropriate.

## Mapping to source files

- Entry point: `egg_incubator_v2/egg_incubator_v2.ino`
- Supervisor and high-level orchestration: `task_control.cpp`,
	`incubator_logic.cpp`
- Temperature/humidity control: `climate_logic.cpp`, `task_climate_control.cpp`
- Sensor aggregation: `task_sensors.cpp`
- UI FSM: `task_ui.cpp`, `oled_ui.cpp`
- Actuator manager and globals: `globals.cpp`, `globals.h`
- Persistence and scheduling: `task_incubator.cpp` (NVS interactions)

When making code changes, prefer updating the small FSM module and its unit
tests rather than altering cross-cutting infrastructure.

## Testing and verification

Unit testing approach:
- Isolate FSM logic from hardware by injecting a fake sensor interface and a
	mock actuator queue.
- Drive `fsm_handle_event()` with deterministic inputs and assert state
	transitions and emitted commands.

Integration testing:
- Use the serial monitor and log statements to validate runtime transitions.
- Create test scripts that replay recorded sensor sequences to verify
	long-running behavior (e.g., incubation cycle).

Fault injection:
- Simulate sensor dropouts, invalid readings, and actuator failures to
	validate fault handling and alarms.

## Examples and patterns

- Maintain small, focused transition tables per FSM.
- Keep step/update loops idempotent: calling `fsm_step()` repeatedly with the
	same inputs should not produce side-effects beyond expected time-driven
	behavior.

## Maintenance notes

- Document state names and transition rationale in the source file headers and
	keep the documentation synchronized with significant logic changes.
- Consider adding a test helper that dumps current FSM states and timers to
	the serial console for easier field troubleshooting.

---

➡️ Next: **Software → Profiles**
