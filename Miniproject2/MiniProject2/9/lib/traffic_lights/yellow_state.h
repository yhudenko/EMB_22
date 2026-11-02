#ifndef LIB_TRAFFIC_LIGHTS_YELLOW_STATE_H
#define LIB_TRAFFIC_LIGHTS_YELLOW_STATE_H

#include "configuration.h"
#include "static_state.h"

class yellow_state : public static_state {
public:
  yellow_state() : static_state(YELLOW_PIN, YELLOW_LIGHT_DURATION_MS) {}
};

#endif // LIB_TRAFFIC_LIGHTS_YELLOW_STATE_H
