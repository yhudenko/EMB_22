#ifndef LIB_TRAFFIC_LIGHTS_YELLOW_BLINKING_STATE_H
#define LIB_TRAFFIC_LIGHTS_YELLOW_BLINKING_STATE_H

#include "configuration.h"
#include "adapter/pin.hpp"
#include "state.h"
#include "adapter/time.hpp"

// Blinks forever: update() never returns COMPLETED.
class yellow_blinking_state : public state {
private:
  const uint32_t blink_cycle_duration_in_ms = YELLOW_BLINK_DURATION_MS * 2;

  eaf::pin yellow_pin;
  uint32_t start_time_in_ms;

public:
  yellow_blinking_state()
      : yellow_pin(YELLOW_PIN, eaf::pin_mode::OUTPUT_MODE),
        start_time_in_ms(0) {}

  void init() override {}

  void start() override {
    yellow_pin.set_high();
    start_time_in_ms = eaf::get_current_time_in_milliseconds();
  }

  void turn_off() override { yellow_pin.set_low(); }

  state_result update() override {
    uint32_t elapsed_time_in_ms =
        eaf::get_current_time_in_milliseconds() - start_time_in_ms;

    if ((elapsed_time_in_ms % blink_cycle_duration_in_ms) <
        YELLOW_BLINK_DURATION_MS) {
      yellow_pin.set_high();
    } else {
      yellow_pin.set_low();
    }

    return state_result::WAIT;
  }
};

#endif // LIB_TRAFFIC_LIGHTS_YELLOW_BLINKING_STATE_H
