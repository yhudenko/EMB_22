#ifndef LIB_TRAFFIC_LIGHTS_RED_YELLOW_STATE_H
#define LIB_TRAFFIC_LIGHTS_RED_YELLOW_STATE_H

#include "configuration.h"
#include "duration_counter/duration_counter.h"
#include "static_state.h"

// Decorates a red static_state with a yellow one so both lights run together.
class red_yellow_state : public state {
private:
  static_state red;
  static_state yellow;
  duration_counter::duration_counter counter;
  const uint32_t duration_in_ms;
  uint32_t start_time_in_ms;
  void update_counter(unsigned long now);

public:
  red_yellow_state()
      : red(RED_PIN, YELLOW_LIGHT_DURATION_MS),
        yellow(YELLOW_PIN, YELLOW_LIGHT_DURATION_MS),
        duration_in_ms(YELLOW_LIGHT_DURATION_MS) {}

  void init() override;

  void start() override;

  void turn_off() override;

  state_result update() override;
};

#endif // LIB_TRAFFIC_LIGHTS_RED_YELLOW_STATE_H
