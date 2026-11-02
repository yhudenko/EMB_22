#ifndef LIB_TRAFFIC_LIGHTS_GREEN_STATE_H
#define LIB_TRAFFIC_LIGHTS_GREEN_STATE_H

#include "configuration.h"
#include "static_state.h"

class green_state : public static_state {
public:
  green_state() : static_state(GREEN_PIN, GREEN_LIGHT_DURATION_MS) {}
};

#endif // LIB_TRAFFIC_LIGHTS_GREEN_STATE_H
