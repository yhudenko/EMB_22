#include "static_state.h"

#include "adapter/time.hpp"

void static_state::init() {}

void static_state::start() {
  pin.set_high();
  start_time_in_ms = eaf::get_current_time_in_milliseconds();
}

void static_state::turn_off() { pin.set_low(); }

state_result static_state::update() {
  uint32_t current_time_in_ms = eaf::get_current_time_in_milliseconds();
  update_counter(current_time_in_ms);
  if (current_time_in_ms - start_time_in_ms >= duration_in_ms) {
    turn_off();
    return state_result::COMPLETED;
  }
  return state_result::WAIT;
}

void static_state::update_counter(unsigned long now) {
  auto time_left = duration_in_ms - (now - start_time_in_ms);
  counter.set_value(time_left);
}
