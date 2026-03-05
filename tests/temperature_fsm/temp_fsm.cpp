#include "temp_fsm.h"

TempFsm::TempFsm(const temp_fsm_cfg_t &cfg) : cfg_(cfg), state_(TEMP_IDLE) {}

void TempFsm::reset() { state_ = TEMP_IDLE; }

void TempFsm::step(float temperature, bool sensor_ok) {
  if (!sensor_ok) {
    state_ = TEMP_FAULT;
    return;
  }

  if (temperature < cfg_.setpoint - cfg_.hysteresis) {
    state_ = TEMP_HEATING;
  } else if (temperature > cfg_.setpoint + cfg_.hysteresis) {
    state_ = TEMP_COOLING;
  } else {
    state_ = TEMP_IDLE;
  }
}

temp_state_t TempFsm::state() const { return state_; }
