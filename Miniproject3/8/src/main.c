#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "melody.h"
#include "player.h"
#include "selector.h"

static const char* TAG = "APP";

static melody_handle_t melody_handle;
static player_handle_t melody_player_handle;
static selector_handle_t option_selector_handle;
static display_handle_t display_handle;
display_update_data_t display_update_data;

static operation_result_t cleanup_resources(void);
static operation_result_t run_application(void);
static operation_result_t get_melody_and_mark_processed(
    selector_handle_t selector, melody_id_t melody,
    melody_handle_t* melody_handle);
static operation_result_t stop_and_release_melody(
    player_handle_t player_handle, melody_handle_t* melody_handle);
static melody_id_t get_melody_id_from_selected_option(
    const selection_result_t* selection);
static inline operation_result_t check_selection(
    const selection_result_t* selection);
static inline void refresh_display(const selection_result_t* selection);
static inline operation_result_t take_selected_melody(
    const selection_result_t* selection);

void app_main(void) {
  ESP_LOGI(TAG, "Starting application");

  if (run_application() != OPERATION_RESULT_OK) {
    ESP_LOGE(TAG, "Application stopped after an error");
  }
}

static operation_result_t run_application(void) {
  operation_result_t result =
      initialize_selector(melody_list_size, &option_selector_handle);
  OPERATION_RETURN_ON_OPERATION_ERROR_WITH_CALLBACK(
      result, cleanup_resources, TAG, "Failed to initialize selector");

  result = initialize_player(&melody_player_handle);
  OPERATION_RETURN_ON_OPERATION_ERROR_WITH_CALLBACK(
      result, cleanup_resources, TAG, "Failed to initialize player");

  display_update_data.options = melody_names;
  display_update_data.option_count = melody_list_size;
  display_update_data.preselected_option = 0;
  display_update_data.selected_option = DISPLAY_NOT_SELECTED_OPTION;

  result = initialize_display(&display_handle);
  if (result != OPERATION_RESULT_OK) {
    ESP_LOGE(TAG, "Failed to initialize display");
  }

  if (update_display(display_handle, &display_update_data) !=
      OPERATION_RESULT_OK) {
    ESP_LOGE(TAG, "Failed to update display");
  }

  while (true) {
    selection_result_t selection = {.state = OPTION_NOT_SELECTED};

    if (melody_handle) {
      OPERATION_RETURN_ON_OPERATION_ERROR_WITH_CALLBACK(
          play_melody(melody_player_handle, melody_handle), cleanup_resources,
          TAG, "Failed to play melody");

      while (is_melody_playing(melody_player_handle, melody_handle)) {
        selection = get_selector_state(option_selector_handle);
        OPERATION_RETURN_ON_OPERATION_ERROR_WITH_CALLBACK(
            check_selection(&selection), cleanup_resources, TAG,
            "Failed to get selector state");

        refresh_display(&selection);

        if (selection.state == OPTION_SELECTED) {
          OPERATION_RETURN_ON_OPERATION_ERROR_WITH_CALLBACK(
              stop_and_release_melody(melody_player_handle, &melody_handle),
              cleanup_resources, TAG, "Failed to stop and release melody");
          break;
        }
        vTaskDelay(1);
      }

      if (melody_handle) {
        OPERATION_RETURN_ON_OPERATION_ERROR_WITH_CALLBACK(
            stop_and_release_melody(melody_player_handle, &melody_handle),
            cleanup_resources, TAG, "Failed to stop and release melody");
      }

      refresh_display(&selection);

      OPERATION_RETURN_ON_OPERATION_ERROR_WITH_CALLBACK(
          take_selected_melody(&selection), cleanup_resources, TAG,
          "Failed to get melody and mark selection processed");
    } else {
      selection = get_selector_state(option_selector_handle);
      OPERATION_RETURN_ON_OPERATION_ERROR_WITH_CALLBACK(
          check_selection(&selection), cleanup_resources, TAG,
          "Failed to get selector state");

      refresh_display(&selection);

      OPERATION_RETURN_ON_OPERATION_ERROR_WITH_CALLBACK(
          take_selected_melody(&selection), cleanup_resources, TAG,
          "Failed to get melody and mark selection processed");
      vTaskDelay(1);
    }
  }
}

static operation_result_t cleanup_resources(void) {
  operation_result_t result = OPERATION_RESULT_OK;

  if (display_handle) {
    if (release_display(display_handle) == OPERATION_RESULT_OK) {
      display_handle = 0;
    } else {
      result = OPERATION_RESULT_ERROR;
    }
  }

  if (melody_player_handle) {
    if (release_player(melody_player_handle) == OPERATION_RESULT_OK) {
      melody_player_handle = NULL;
    } else {
      result = OPERATION_RESULT_ERROR;
    }
  }

  if (melody_handle && !melody_player_handle) {
    if (release_melody(melody_handle) == OPERATION_RESULT_OK) {
      melody_handle = NULL;
    } else {
      result = OPERATION_RESULT_ERROR;
    }
  }

  if (option_selector_handle) {
    if (release_selector(option_selector_handle) == OPERATION_RESULT_OK) {
      option_selector_handle = NULL;
    } else {
      result = OPERATION_RESULT_ERROR;
    }
  }

  if (result != OPERATION_RESULT_OK) {
    ESP_LOGE(TAG, "Failed to clean up one or more application resources");
  }

  return result;
}

static operation_result_t get_melody_and_mark_processed(
    selector_handle_t selector_handle, melody_id_t melody,
    melody_handle_t* melody_handle) {
  OPERATION_PROPAGATE_OPERATION_ERROR(get_melody_by_id(melody, melody_handle));

  return mark_selection_processed(selector_handle);
}

static operation_result_t stop_and_release_melody(
    player_handle_t player_handle, melody_handle_t* melody_handle) {
  if (!player_handle || !melody_handle || !*melody_handle) {
    return OPERATION_RESULT_ERROR;
  }

  OPERATION_PROPAGATE_OPERATION_ERROR(
      stop_melody(player_handle, *melody_handle));

  OPERATION_PROPAGATE_OPERATION_ERROR(release_melody(*melody_handle));

  *melody_handle = NULL;
  return OPERATION_RESULT_OK;
}

static melody_id_t get_melody_id_from_selected_option(
    const selection_result_t* selection) {
  return selection->selected_option + 1;
}

static inline operation_result_t check_selection(
    const selection_result_t* selection) {
  return selection->state == OPTION_ERROR ? OPERATION_RESULT_ERROR
                                          : OPERATION_RESULT_OK;
}

static inline void refresh_display(const selection_result_t* selection) {
  display_update_data.selected_option = selection->state == OPTION_SELECTED
                                            ? selection->selected_option
                                            : DISPLAY_NOT_SELECTED_OPTION;
  display_update_data.preselected_option = selection->preselected_option;

  if (update_display(display_handle, &display_update_data) !=
      OPERATION_RESULT_OK) {
    ESP_LOGE(TAG, "Failed to update display");
  }
}

static inline operation_result_t take_selected_melody(
    const selection_result_t* selection) {
  if (selection->state != OPTION_SELECTED) {
    return OPERATION_RESULT_OK;
  }

  return get_melody_and_mark_processed(
      option_selector_handle, get_melody_id_from_selected_option(selection),
      &melody_handle);
}
