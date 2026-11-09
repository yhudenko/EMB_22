#ifndef DISPLAY_H
#define DISPLAY_H

#include <stddef.h>

#include "errors.h"

#define DISPLAY_NOT_SELECTED_OPTION -1

struct display_update_data {
  const char* const* options;
  size_t option_count;
  int preselected_option;
  int selected_option;
};

typedef int display_handle_t;
typedef struct display_update_data display_update_data_t;

operation_result_t initialize_display(display_handle_t* handle);
operation_result_t release_display(display_handle_t handle);
operation_result_t update_display(display_handle_t handle,
                                  const display_update_data_t* update_data);

#endif  // DISPLAY_H