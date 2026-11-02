#include "toggle_switch.h"

#include "adapter/interrupt.hpp"
#include "adapter/utils.hpp"
#include "esp_attr.h"

toggle_switch *toggle_switch::instance = nullptr;

toggle_switch::toggle_switch(uint8_t pin_index)
    : pin_index(pin_index), current_state(switch_state::OFF),
      button_pin(pin_index, eaf::pin_mode::INPUT_PULLUP_MODE) {}

void toggle_switch::init() {
  instance = this;
  eaf::attach_interrupt(pin_index, isr, eaf::voltage_state::FALLING_STATE);
}

switch_state toggle_switch::update() { return current_state; }

IRAM_LOCATED void toggle_switch::isr() {
  if (instance == nullptr) {
    return;
  }
  instance->current_state = instance->current_state == switch_state::OFF
                                ? switch_state::ON
                                : switch_state::OFF;
}
