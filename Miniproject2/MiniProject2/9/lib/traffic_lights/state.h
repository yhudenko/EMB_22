#ifndef LIB_TRAFFIC_LIGHTS_STATE_H
#define LIB_TRAFFIC_LIGHTS_STATE_H

enum class state_result { WAIT, COMPLETED };

class state {
public:
  state() = default;
  virtual ~state() = default;

  virtual void init() = 0;
  virtual void start() = 0;
  virtual void turn_off() = 0;
  virtual state_result update() = 0;

protected:
  uint32_t start_time_in_ms;
};

#endif // LIB_TRAFFIC_LIGHTS_STATE_H