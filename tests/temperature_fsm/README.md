# Temperature FSM unit-test (host)

This folder contains a small, self-contained unit-test scaffolding for the
Temperature FSM. It is designed to run on the host (desktop) using a standard
C++ toolchain (g++ / clang++).

Build and run

```bash
cd tests/temperature_fsm
g++ -std=c++17 -O2 temp_fsm.cpp test_temperature_fsm.cpp -o temp_fsm_test
./temp_fsm_test
```

What this does
- `temp_fsm.cpp` / `temp_fsm.h` implement a minimal, testable Temperature FSM
  API compatible with the design docs.
- `test_temperature_fsm.cpp` contains deterministic test cases (sensor invalid,
  below setpoint, above setpoint, hysteresis behavior) and reports failures.

Notes
- This is scaffolding for unit testing and demonstration. When integrating with
  the firmware, adapt the FSM API to the project's logger, timing, and
  actuator interfaces.
