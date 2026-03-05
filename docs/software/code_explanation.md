# Code Walkthrough

This page summarizes the firmware code structure and explains where to find
the primary implementation points for the incubator firmware in
`egg_incubator_v2/`.

Purpose
- Provide a concise mapping from architecture to source files.
- Explain key modules and the responsibilities of major source files.
- Give practical guidance for debugging, extending, and building the
  firmware.

Project entry point
- `egg_incubator_v2/egg_incubator_v2.ino`
  - Initializes hardware, tasks, and system-wide resources.
  - Performs early configuration (RTC, NVS, watchdog, and logging).

Global state and configuration
- `egg_incubator_v2/config.h` — compile-time configuration and defaults.
- `egg_incubator_v2/secrets.h.example` — example secrets; copy to `secrets.h`
  and populate to enable cloud features.
- `egg_incubator_v2/globals.h` / `globals.cpp` — global structures, shared
  state (protected by mutexes), and helper utilities.

Task-based decomposition
- Tasks are organized to follow the software architecture described in the
  documentation: sensors, control, actuators, UI, RTC, cloud, and logging.
- Key task files:
  - `task_sensors.cpp` — sensor reads, validation, and publishing to queues.
  - `task_climate_control.cpp` — implements control FSMs for temperature and
    humidity regulation.
  - `task_incubator.cpp` — higher-level incubator sequencing (turner logic,
    scheduled events, NVS persistence of timestamps).
  - `task_ui.cpp` / `oled_ui.cpp` — UI FSM and OLED rendering primitives.
  - `task_wifi_manager.cpp` and `task_cloud.cpp` — Wi‑Fi lifecycle and cloud
    telemetry uploads.

Control logic and FSMs
- Control logic is implemented as finite-state machines (FSMs) to keep
  behavior explicit and testable.
- Primary FSM implementations live in `climate_logic.cpp`, `incubator_logic.cpp`,
  and `fsm`-named helper modules. Each FSM module exposes a clear step/update
  function consumed by the corresponding task.

Actuators and HAL
- Actuator access is centralized to avoid conflicting writes:
  - Relay outputs and PWM fan control are coordinated via `globals.*` and
    `setFanSpeed()` / `setRelay()` helpers.
  - GPIO pin assignments are grouped in `globals.h` and documented in
    `egg_incubator_v2/config.h`.

Persistence
- NVS (non-volatile storage) is used for storing persistent state such as
  last-turn timestamps, saved profiles, and configuration overrides. Look for
  NVS API usage in `task_incubator.cpp` and `globals.cpp`.

Telemetry & Cloud
- Cloud interactions are isolated in `task_cloud.cpp`. The code reads
  telemetry objects and performs HTTP(S) uploads. To enable cloud, populate
  `secrets.h` and `CLOUD_ROOT_CA` as needed.

Build and run
- Build in Arduino IDE or PlatformIO. Recommended quick steps:

```bash
# Using Arduino CLI (command-line recommended)
cd egg_incubator_v2
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli compile --fqbn esp32:esp32:esp32dev
arduino-cli upload --fqbn esp32:esp32:esp32dev --port COM3
```

Alternative methods:

```bash
# Arduino IDE: open the sketch and use the IDE build/upload buttons

# PlatformIO (reproducible builds)
cd egg_incubator_v2
platformio run --target upload
```

Debugging tips
- Use the serial monitor (default 115200) for boot logs and periodic telemetry.
- Inspect `task_*` files for task-specific logging. Add log lines guarded by
  `#ifdef DEBUG` where appropriate.
- To trace FSM behavior, instrument state transitions with concise logs.

Extending the firmware
- Add new sensors by creating a driver in `task_sensors.cpp` and exposing
  a unified sensor health + data structure consumed by control tasks.
- Add actuators via the actuator HAL and avoid direct digital writes from
  multiple modules; always use the centralized helpers.

References
- Firmware entry: `egg_incubator_v2/egg_incubator_v2.ino`
- Globals & config: `egg_incubator_v2/globals.h`, `egg_incubator_v2/config.h`
- Tasks: files prefixed with `task_*.cpp`
- UI: `oled_ui.cpp`, `task_ui.cpp`

If you want, I can:
- Expand this walkthrough into file-by-file documentation with code snippets.
- Add cross-links from each code file to this page by updating source headers.
