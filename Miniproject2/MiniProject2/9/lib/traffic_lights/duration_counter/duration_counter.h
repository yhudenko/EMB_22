#pragma once

#include <cstdint>

#include "adapter/pin.hpp"

#define FIRST_DIGIT_PIN 42
#define SECOND_DIGIT_PIN 2

#define SEGMENT_A_PIN 4
#define SEGMENT_B_PIN 5
#define SEGMENT_C_PIN 6
#define SEGMENT_D_PIN 7
#define SEGMENT_E_PIN 15
#define SEGMENT_F_PIN 35
#define SEGMENT_G_PIN 36
#define SEGMENT_DP_PIN 37

namespace duration_counter {

enum segment_mask : uint8_t {
  SEGMENT_A = 1u << 0,
  SEGMENT_B = 1u << 1,
  SEGMENT_C = 1u << 2,
  SEGMENT_D = 1u << 3,
  SEGMENT_E = 1u << 4,
  SEGMENT_F = 1u << 5,
  SEGMENT_G = 1u << 6,
  SEGMENT_DP = 1u << 7,
};

constexpr uint8_t DIGIT_0_MASK =
    SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_E | SEGMENT_F;
constexpr uint8_t DIGIT_1_MASK = SEGMENT_B | SEGMENT_C;
constexpr uint8_t DIGIT_2_MASK =
    SEGMENT_A | SEGMENT_B | SEGMENT_D | SEGMENT_E | SEGMENT_G;
constexpr uint8_t DIGIT_3_MASK =
    SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_G;
constexpr uint8_t DIGIT_4_MASK = SEGMENT_B | SEGMENT_C | SEGMENT_F | SEGMENT_G;
constexpr uint8_t DIGIT_5_MASK =
    SEGMENT_A | SEGMENT_C | SEGMENT_D | SEGMENT_F | SEGMENT_G;
constexpr uint8_t DIGIT_6_MASK = DIGIT_5_MASK | SEGMENT_E;
constexpr uint8_t DIGIT_7_MASK = SEGMENT_A | SEGMENT_B | SEGMENT_C;
constexpr uint8_t DIGIT_8_MASK = DIGIT_0_MASK | SEGMENT_G;
constexpr uint8_t DIGIT_9_MASK = DIGIT_4_MASK | SEGMENT_A | SEGMENT_D;

constexpr uint8_t DIGIT_MASKS[] = {
    DIGIT_0_MASK, DIGIT_1_MASK, DIGIT_2_MASK, DIGIT_3_MASK, DIGIT_4_MASK,
    DIGIT_5_MASK, DIGIT_6_MASK, DIGIT_7_MASK, DIGIT_8_MASK, DIGIT_9_MASK,
};

struct display_data {
  uint8_t tens;
  uint8_t ones;
};

class duration_counter {
public:
  duration_counter();
  void set_value(unsigned long value);

private:
  void update_display(const display_data &data);
  void update_digit(eaf::pin &digit_pin, uint8_t digit);
  void calculate_values(unsigned long value, display_data *data);
  void clear_display();
  bool is_ones_update;

  eaf::pin first_digit_pin;
  eaf::pin second_digit_pin;
  eaf::pin segment_a_pin;
  eaf::pin segment_b_pin;
  eaf::pin segment_c_pin;
  eaf::pin segment_d_pin;
  eaf::pin segment_e_pin;
  eaf::pin segment_f_pin;
  eaf::pin segment_g_pin;
  eaf::pin segment_dp_pin;
  eaf::pin *segment_pins[8];
};

} // namespace duration_counter