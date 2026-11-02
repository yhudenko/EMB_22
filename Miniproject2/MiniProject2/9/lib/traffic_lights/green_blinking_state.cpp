#include "green_blinking_state.h"

void green_blinking_state::start() {
  green_pin.set_high();
  start_time_in_ms = eaf::get_current_time_in_milliseconds();
}

state_result green_blinking_state::update() {

  uint32_t current_time_in_ms = eaf::get_current_time_in_milliseconds();

  update_counter(current_time_in_ms);

  uint32_t elapsed_time_in_ms = current_time_in_ms - start_time_in_ms;

  if (elapsed_time_in_ms >= total_blink_duration_in_ms) {
    turn_off();
    return state_result::COMPLETED;
  }

  if ((elapsed_time_in_ms % blink_cycle_duration_in_ms) <
      GREEN_BLINK_DURATION_MS) {
    green_pin.set_high();
  } else {
    green_pin.set_low();
  }

  return state_result::WAIT;
}

void green_blinking_state::turn_off() { green_pin.set_low(); }

void green_blinking_state::update_counter(unsigned long now) {
  auto time_left = total_blink_duration_in_ms - (now - start_time_in_ms);
  counter.set_value(time_left);
}