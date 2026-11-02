#include <Arduino.h>

#include "led_group.h"
#include "logger.h"
#include "seven_segment_display.h"
#include "time_base.h"
#include "traffic_fsm.h"

namespace {

TrafficFsm g_fsm;
LedGroup g_leds;
SevenSegmentDisplay g_display;

uint16_t g_lastLoggedSecond = 0;

void logTransition() {
    Logger::linef("-> %s (%us) mask=RYG:%u%u%u", g_fsm.phaseName(),
                  static_cast<unsigned>(g_fsm.phaseDurationMs() / 1000),
                  (g_fsm.lampMask() & phases::kMaskRed) ? 1 : 0,
                  (g_fsm.lampMask() & phases::kMaskYellow) ? 1 : 0,
                  (g_fsm.lampMask() & phases::kMaskGreen) ? 1 : 0);
}

}  // namespace

void setup() {
    Logger::begin(115200);
    delay(1000);

    g_leds.begin();
    g_display.begin();
    TimeBase::begin();
    g_fsm.reset();

    Logger::linef("Traffic Light boot | env=traffic_light | phases=%u",
                  static_cast<unsigned>(phases::kPhaseCount));
    logTransition();

    g_leds.write(g_fsm.lampMask());
    g_lastLoggedSecond = g_fsm.remainingSeconds();
}

void loop() {
    const uint32_t deltaMs = TimeBase::takeElapsedMs();
    if (deltaMs > 0) {
        const uint8_t prevMask = g_fsm.lampMask();
        const bool transitioned = g_fsm.advance(deltaMs);

        if (transitioned) {
            logTransition();
            g_lastLoggedSecond = g_fsm.remainingSeconds();
            Logger::linef("%s remaining=%us", g_fsm.phaseName(),
                static_cast<unsigned>(g_lastLoggedSecond));
            g_display.setNumber(g_lastLoggedSecond);
        }

        const uint8_t mask = g_fsm.lampMask();
        if (mask != prevMask) {
            g_leds.write(mask);
        }
    }

    g_display.scan();
}
