#ifndef SELECTOR_H
#define SELECTOR_H

#include <stddef.h>

#include "errors.h"
#include "melody.h"

typedef int* selector_handle_t;

enum selection_state { OPTION_NOT_SELECTED, OPTION_SELECTED, OPTION_ERROR };

struct selection_result {
  enum selection_state state;
  int preselected_option;
  int selected_option;
};

typedef struct selection_result selection_result_t;

operation_result_t initialize_selector(size_t melody_id_count,
                                       selector_handle_t* handle);
selection_result_t get_selector_state(selector_handle_t handle);
operation_result_t mark_selection_processed(selector_handle_t handle);
operation_result_t release_selector(selector_handle_t handle);

#endif  // SELECTOR_H