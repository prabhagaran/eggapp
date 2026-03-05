// Minimal, testable Temperature FSM interface
#pragma once

#include <cstdint>

typedef enum { TEMP_IDLE, TEMP_HEATING, TEMP_COOLING, TEMP_FAULT } temp_state_t;

typedef struct {
  float setpoint;
  float hysteresis;
  uint32_t min_on_ms; // minimum on time for actuators (not enforced in host test)
} temp_fsm_cfg_t;

class TempFsm {
public:
  explicit TempFsm(const temp_fsm_cfg_t &cfg);
  void reset();
  void step(float temperature, bool sensor_ok);
  temp_state_t state() const;

private:
  temp_fsm_cfg_t cfg_;
  temp_state_t state_;
};
