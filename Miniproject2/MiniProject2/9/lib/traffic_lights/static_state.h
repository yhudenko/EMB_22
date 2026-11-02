#ifndef LIB_TRAFFIC_LIGHTS_STATIC_STATE_H
#define LIB_TRAFFIC_LIGHTS_STATIC_STATE_H

#include "duration_counter/duration_counter.h"
#include <stdint.h>

#include "adapter/pin.hpp"
#include "state.h"

class static_state : public state {
private:
  const uint8_t pin_index;
  const uint32_t duration_in_ms;
  uint32_t start_time_in_ms;
  eaf::pin pin;

protected:
  duration_counter::duration_counter counter;
  void update_counter(unsigned long now);

public:
  static_state(uint8_t pin_index, uint32_t duration_in_ms)
      : pin_index(pin_index), duration_in_ms(duration_in_ms),
        start_time_in_ms(0), pin(pin_index, eaf::pin_mode::OUTPUT_MODE) {}

  void init() override;
  void start() override;
  void turn_off() override;
  state_result update() override;
};

#endif // LIB_TRAFFIC_LIGHTS_STATIC_STATE_H
