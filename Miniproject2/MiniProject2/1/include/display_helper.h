#pragma once
#include <Arduino.h>
#include <seven_segment_config.h>

void setPins(const uint8_t *pins, uint8_t n, bool val);

void disableAllDisplays();

void digitalWriteReverse(uint8_t pin, bool val);

void writeSegments(uint8_t digit);

void showDigit(uint8_t activeDisplayIndex, int digit);