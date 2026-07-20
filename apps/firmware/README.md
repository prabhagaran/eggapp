# egg-incubator-esp32-rtos

Overview
This repository provides firmware, documentation, and supporting materials for
an ESP32-based egg incubator controller. The firmware is organized as a
multi-task (FreeRTOS-style) application responsible for sensor acquisition,
climate control (temperature and humidity), actuator control (fan, heater,
turner), user interface, real-time clock integration, and optional cloud
telemetry.

Key features
- Precise temperature and humidity control with configurable profiles.
- PWM-controlled fan and relay-managed heater and turner.
- Local OLED-based user interface with menu navigation.
- Persistent settings using NVS and optional cloud telemetry support.

Repository structure
- `docs/` — design documents, user guides, and developer notes (MkDocs-ready).
- `egg_incubator_v2/` — primary firmware workspace containing the main
	sketch, C++ sources and headers, and a workspace-level `CHANGELOG.md`.
- `egg_incubato_esp32_rtos/` — legacy sketches and UI experiments.

Quick start
1. Copy `egg_incubator_v2/secrets.h.example` to `egg_incubator_v2/secrets.h`
	 and populate cloud endpoint, authentication token, and optional TLS root CA.
2. Build and upload using one of the following methods:

- Arduino CLI (recommended if you use command-line flows):

```bash
# Install board core and dependencies once
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Build and upload (example board: esp32:esp32:esp32dev)
cd egg_incubator_v2
arduino-cli compile --fqbn esp32:esp32:esp32dev
arduino-cli upload --fqbn esp32:esp32:esp32dev --port COM3
```

- Arduino IDE: open `egg_incubator_v2/egg_incubator_v2.ino`, select board
  and port, then build and upload through the IDE.

- PlatformIO: open the folder as a PlatformIO project and run the upload
  target. Example:

```bash
cd egg_incubator_v2
platformio run --target upload
```

3. Open a serial monitor (default 115200 baud) to observe boot diagnostics and
	telemetry output.

Configuration
- Hardware pin assignments and configurable options are defined in
	`egg_incubator_v2/config.h` and `globals.h`.
- Secrets and cloud tokens are intentionally excluded from source control.

Documentation and changelog
- The `docs/` directory contains architecture and usage documentation.
- Firmware history and notable fixes live in
	`egg_incubator_v2/CHANGELOG.md`.

Contributing
- Please file issues for bugs or enhancements. For code changes, open a pull
	request and reference related issues.

License and contact
- If a `LICENSE` file exists, it governs use of this repository. For
	licensing questions or reuse permissions, contact the repository owner.

Next steps I can help with
- Add a `platformio.ini` for reproducible builds and CI.
- Create a concise `egg_incubator_v2/README.md` documenting build
	dependencies and local testing steps.
