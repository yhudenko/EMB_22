#include "duration_counter.h"

namespace duration_counter {

duration_counter::duration_counter()
    : is_ones_update(false),
      first_digit_pin(FIRST_DIGIT_PIN, eaf::pin_mode::OUTPUT_MODE),
      second_digit_pin(SECOND_DIGIT_PIN, eaf::pin_mode::OUTPUT_MODE),
      segment_a_pin(SEGMENT_A_PIN, eaf::pin_mode::OUTPUT_MODE),
      segment_b_pin(SEGMENT_B_PIN, eaf::pin_mode::OUTPUT_MODE),
      segment_c_pin(SEGMENT_C_PIN, eaf::pin_mode::OUTPUT_MODE),
      segment_d_pin(SEGMENT_D_PIN, eaf::pin_mode::OUTPUT_MODE),
      segment_e_pin(SEGMENT_E_PIN, eaf::pin_mode::OUTPUT_MODE),
      segment_f_pin(SEGMENT_F_PIN, eaf::pin_mode::OUTPUT_MODE),
      segment_g_pin(SEGMENT_G_PIN, eaf::pin_mode::OUTPUT_MODE),
      segment_dp_pin(SEGMENT_DP_PIN, eaf::pin_mode::OUTPUT_MODE) {
  segment_pins[0] = &segment_a_pin;
  segment_pins[1] = &segment_b_pin;
  segment_pins[2] = &segment_c_pin;
  segment_pins[3] = &segment_d_pin;
  segment_pins[4] = &segment_e_pin;
  segment_pins[5] = &segment_f_pin;
  segment_pins[6] = &segment_g_pin;
  segment_pins[7] = &segment_dp_pin;
  clear_display();
}

void duration_counter::set_value(unsigned long value) {
  clear_display();
  display_data data;
  calculate_values(value, &data);
  update_display(data);
}

void duration_counter::calculate_values(unsigned long value,
                                        display_data *data) {
  auto seconds_left = (value / 1000) % 100;
  data->tens = seconds_left / 10;
  data->ones = seconds_left % 10;
}

void duration_counter::clear_display() {
  first_digit_pin.set_low();
  second_digit_pin.set_low();

  segment_a_pin.set_high();
  segment_b_pin.set_high();
  segment_c_pin.set_high();
  segment_d_pin.set_high();
  segment_e_pin.set_high();
  segment_f_pin.set_high();
  segment_g_pin.set_high();
  segment_dp_pin.set_high();
}

void duration_counter::update_display(const display_data &data) {
  if (is_ones_update) {
    update_digit(first_digit_pin, data.ones);
    is_ones_update = false;
  } else {
    update_digit(second_digit_pin, data.tens);
    is_ones_update = true;
  }
}

void duration_counter::update_digit(eaf::pin &digit_pin, uint8_t digit) {
  const uint8_t mask = DIGIT_MASKS[digit];

  for (uint8_t bit = 0; bit < sizeof(segment_pins) / sizeof(segment_pins[0]);
       ++bit) {
    if (mask & (1u << bit)) {
      segment_pins[bit]->set_low();
    } else {
      segment_pins[bit]->set_high();
    }
  }

  digit_pin.set_high();
}

} // namespace duration_counter