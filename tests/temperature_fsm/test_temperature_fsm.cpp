#include <iostream>
#include <vector>
#include <string>
#include "temp_fsm.h"

static void expect(bool cond, const std::string &msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << "\n";
    std::exit(1);
  }
}

int main() {
  temp_fsm_cfg_t cfg{.setpoint = 37.5f, .hysteresis = 0.5f, .min_on_ms = 30000};
  TempFsm fsm(cfg);

  // Test 1: sensor invalid -> FAULT
  fsm.reset();
  fsm.step(37.5f, false);
  expect(fsm.state() == TEMP_FAULT, "Sensor invalid should set TEMP_FAULT");

  // Test 2: below setpoint -> HEATING
  fsm.reset();
  fsm.step(36.0f, true); // 36.0 < 37.5 - 0.5
  expect(fsm.state() == TEMP_HEATING, "Below setpoint should set TEMP_HEATING");

  // Test 3: above setpoint -> COOLING
  fsm.reset();
  fsm.step(39.0f, true); // 39.0 > 37.5 + 0.5
  expect(fsm.state() == TEMP_COOLING, "Above setpoint should set TEMP_COOLING");

  // Test 4: within band -> IDLE
  fsm.reset();
  fsm.step(37.6f, true); // within 37.0..38.0
  expect(fsm.state() == TEMP_IDLE, "Within hysteresis band should set TEMP_IDLE");

  std::cout << "All tests passed.\n";
  return 0;
}
