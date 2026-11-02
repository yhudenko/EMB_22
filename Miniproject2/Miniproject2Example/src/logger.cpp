#include "logger.h"

#include <cstdarg>
#include <cstdio>

namespace {

void writePrefix() {
    const uint32_t ms = millis();
    const uint32_t totalSec = ms / 1000;
    char buf[16];
    snprintf(buf, sizeof(buf), "[%02u:%02u:%02u.%03u] ",
             static_cast<unsigned>(totalSec / 3600),
             static_cast<unsigned>((totalSec / 60) % 60),
             static_cast<unsigned>(totalSec % 60),
             static_cast<unsigned>(ms % 1000));
    Serial0.print(buf);
}

}  // namespace

namespace Logger {

void begin(uint32_t baud) {
    Serial0.begin(baud);
}

void line(const char* msg) {
    writePrefix();
    Serial0.println(msg);
}

void linef(const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    writePrefix();
    Serial0.println(buf);
}

}  // namespace Logger
