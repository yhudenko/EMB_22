#ifndef LIB_TRAFFIC_LIGHTS_GREEN_BLINKING_STATE_H
#define LIB_TRAFFIC_LIGHTS_GREEN_BLINKING_STATE_H

#include "adapter/pin.hpp"
#include "adapter/time.hpp"
#include "configuration.h"
#include "duration_counter/duration_counter.h"
#include "state.h"

class green_blinking_state : public state {
private:
  const uint32_t blink_cycle_duration_in_ms = GREEN_BLINK_DURATION_MS * 2;
  const uint32_t total_blink_duration_in_ms =
      blink_cycle_duration_in_ms * GREEN_BLINK_NUMBER;

  eaf::pin green_pin;
  duration_counter::duration_counter counter;
  void update_counter(unsigned long now);

public:
  green_blinking_state() : green_pin(GREEN_PIN, eaf::pin_mode::OUTPUT_MODE) {}

  void init() override {}

  void start() override;

  void turn_off() override;

  state_result update() override;
};
#endif // LIB_TRAFFIC_LIGHTS_GREEN_BLINKING_STATE_H
