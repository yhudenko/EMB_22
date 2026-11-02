#ifndef LIB_TRAFFIC_LIGHTS_TOGGLE_SWITCH_H
#define LIB_TRAFFIC_LIGHTS_TOGGLE_SWITCH_H

#include <stdint.h>

#include "adapter/pin.hpp"

enum class switch_state { OFF, ON };

// Flips its state on every falling edge of the button pin interrupt.
class toggle_switch {
private:
  static toggle_switch *instance;
  static void isr();

  const uint8_t pin_index;
  volatile switch_state current_state;
  eaf::pin button_pin;

public:
  explicit toggle_switch(uint8_t pin_index);

  void init();
  switch_state update();
};

#endif // LIB_TRAFFIC_LIGHTS_TOGGLE_SWITCH_H
