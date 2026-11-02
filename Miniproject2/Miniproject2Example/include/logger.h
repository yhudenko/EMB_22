#pragma once

#include <Arduino.h>

namespace Logger {

void begin(uint32_t baud);

void line(const char* msg);
void linef(const char* fmt, ...);

}  // namespace Logger
