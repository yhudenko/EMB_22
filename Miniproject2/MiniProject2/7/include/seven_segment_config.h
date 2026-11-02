#pragma once
#include "driver/gpio.h"

#define SEG_A  GPIO_NUM_16
#define SEG_B  GPIO_NUM_17
#define SEG_C  GPIO_NUM_9
#define SEG_D  GPIO_NUM_8
#define SEG_E  GPIO_NUM_18
#define SEG_F  GPIO_NUM_15
#define SEG_G  GPIO_NUM_7
#define SEG_DP GPIO_NUM_10

#define DIG_1  GPIO_NUM_12  
#define DIG_2  GPIO_NUM_11

static const gpio_num_t segment_pins[8] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G, SEG_DP};
static const gpio_num_t digit_pins[2] = {DIG_1, DIG_2};

// Маска для Спільного Анода (0 - сегмент світиться, 1 - не світиться)
static const uint8_t num_patterns[10] = {
    0b11000000, // 0
    0b11111001, // 1
    0b10100100, // 2
    0b10110000, // 3
    0b10011001, // 4
    0b10010010, // 5
    0b10000010, // 6
    0b11111000, // 7
    0b10000000, // 8
    0b10010000  // 9
};