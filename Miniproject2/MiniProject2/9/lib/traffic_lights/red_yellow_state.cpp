#include "red_yellow_state.h"
#include "adapter/time.hpp"

void red_yellow_state::init() {
  red.init();
  yellow.init();
}

void red_yellow_state::start() {
  start_time_in_ms = eaf::get_current_time_in_milliseconds();
  red.start();
  yellow.start();
}

void red_yellow_state::turn_off() {
  red.turn_off();
  yellow.turn_off();
}

state_result red_yellow_state::update() {
  uint32_t current_time_in_ms = eaf::get_current_time_in_milliseconds();
  update_counter(current_time_in_ms);
  state_result red_result = red.update();
  state_result yellow_result = yellow.update();
  if (red_result == state_result::COMPLETED &&
      yellow_result == state_result::COMPLETED) {
    return state_result::COMPLETED;
  }
  return state_result::WAIT;
}

void red_yellow_state::update_counter(unsigned long now) {
  auto time_left = duration_in_ms - (now - start_time_in_ms);
  counter.set_value(time_left);
}
