#include <cstddef>
#include <cstdint>

#include "app.h"
#include "configuration.h"
#include "green_blinking_state.h"
#include "green_state.h"
#include "red_state.h"
#include "red_yellow_state.h"
#include "toggle_switch.h"
#include "yellow_blinking_state.h"
#include "yellow_state.h"

#include "adapter/logger.hpp"
#include "adapter/task.hpp"

namespace {

constexpr uint32_t TASK_STACK_DEPTH = 2048;
constexpr uint8_t TASK_PRIORITY = 5;
constexpr size_t STATES_COUNT = 5;

struct application {
  green_state green;
  green_blinking_state green_blinking;
  yellow_state yellow;
  red_state red;
  red_yellow_state red_yellow;
  yellow_blinking_state yellow_blinking;
  state *states[STATES_COUNT];
  toggle_switch mode_switch;
  size_t current_state_index = 0;
  switch_state current_mode = switch_state::OFF;

  application()
      : states{&green, &green_blinking, &yellow, &red, &red_yellow},
        mode_switch(BUTTON_PIN) {}
};

application &get_application() {
  static application instance;
  return instance;
}

void switch_mode(application &app, switch_state mode) {
  app.states[app.current_state_index]->turn_off();
  app.yellow_blinking.turn_off();
  app.current_mode = mode;
  if (mode == switch_state::ON) {
    app.yellow_blinking.start();
  } else {
    app.current_state_index = 0;
    app.states[app.current_state_index]->start();
  }
}

void run_sequence(application &app) {
  if (app.states[app.current_state_index]->update() ==
      state_result::COMPLETED) {
    app.current_state_index = (app.current_state_index + 1) % STATES_COUNT;
    app.states[app.current_state_index]->start();
  }
}

void update_task(void *) {
  application &app = get_application();
  switch_state requested_mode = app.mode_switch.update();
  if (requested_mode != app.current_mode) {
    switch_mode(app, requested_mode);
  }

  if (app.current_mode == switch_state::ON) {
    app.yellow_blinking.update();
  } else {
    run_sequence(app);
  }
}

eaf::task_context update_task_context;

} // namespace

void app_init() {
  application &app = get_application();
  for (state *s : app.states) {
    s->init();
  }
  app.yellow_blinking.init();
  app.mode_switch.init();
}

void app_run() {
  application &app = get_application();
  app.current_mode = app.mode_switch.update();
  switch_mode(app, app.current_mode);
  update_task_context.task_function = update_task;
  update_task_context.parameters = nullptr;
  if (eaf::schedule_task(&update_task_context, "traffic_lights_update",
                         TASK_STACK_DEPTH,
                         TASK_PRIORITY) == eaf::result::FAILURE) {
    eaf::log_message("Failed to schedule traffic lights update task\n");
  }
}
