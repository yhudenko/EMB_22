#pragma once
#include "driver/gpio.h"

namespace Config {
    constexpr gpio_num_t potGpioPin = GPIO_NUM_1;
    constexpr gpio_num_t servoPin = GPIO_NUM_2;
    constexpr gpio_num_t buzzerPin = GPIO_NUM_21;
    constexpr gpio_num_t encoderSwPin = GPIO_NUM_4;
    constexpr gpio_num_t encoderDtPin = GPIO_NUM_5;
    constexpr gpio_num_t encoderClkPin = GPIO_NUM_6;
    constexpr gpio_num_t buttonLvlPin = GPIO_NUM_38;
    constexpr gpio_num_t buttonStartPin = GPIO_NUM_14;

    constexpr gpio_num_t segAPin = GPIO_NUM_8;
    constexpr gpio_num_t segBPin = GPIO_NUM_9;
    constexpr gpio_num_t segCPin = GPIO_NUM_12;
    constexpr gpio_num_t segDPin = GPIO_NUM_11;
    constexpr gpio_num_t segEPin = GPIO_NUM_10;
    constexpr gpio_num_t segFPin = GPIO_NUM_18;
    constexpr gpio_num_t segGPin = GPIO_NUM_17;
    constexpr gpio_num_t segDpPin = GPIO_NUM_13;

    constexpr gpio_num_t digit1Pin = GPIO_NUM_47;
    constexpr gpio_num_t digit2Pin = GPIO_NUM_15;
    constexpr gpio_num_t digit3Pin = GPIO_NUM_16;

    constexpr uint32_t HASH_LVL1 = 0x000c5540; //5
    constexpr uint32_t HASH_LVL2 = 0x566a3879; //53
    constexpr uint32_t HASH_LVL3 = 0x56cf37ab; //123
}
