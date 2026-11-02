#ifndef LIB_TRAFFIC_LIGHTS_RED_STATE_H
#define LIB_TRAFFIC_LIGHTS_RED_STATE_H

#include "configuration.h"
#include "static_state.h"

class red_state : public static_state {
public:
  red_state() : static_state(RED_PIN, RED_LIGHT_DURATION_MS) {}
};

#endif // LIB_TRAFFIC_LIGHTS_RED_STATE_H
