#include "selector.h"

#include <math.h>

#include "driver/gpio.h"
#include "driver/gpio_filter.h"
#include "driver/pulse_cnt.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"

#define SELECTOR_BUTTON 17
#define SELECTOR_SIGNAL_B 18
#define SELECTOR_SIGNAL_A 8

static const char* TAG = "SELECTOR";

static int selector_handle = 1;
static pcnt_unit_handle_t pcnt_unit;
static pcnt_channel_handle_t pcnt_channel_0;
static pcnt_channel_handle_t pcnt_channel_1;
static gpio_glitch_filter_handle_t glitch_filter_handle;
static size_t melody_list_count;
static bool is_selected;
static bool pcnt_unit_enabled;
static bool pcnt_unit_running;
static bool button_pin_configured;
static bool button_filter_enabled;
static bool button_handler_added;
static bool button_interrupt_enabled;
static bool gpio_isr_service_owned;

static void on_selector_button_clicked(void* arg);
static operation_result_t setup_encoder_button(void);
static operation_result_t setup_encoder_selector(size_t melody_id_count);
static operation_result_t release_selector_resources(void);

operation_result_t initialize_selector(size_t melody_id_count,
                                       selector_handle_t* handle) {
  if (!handle || *handle || melody_id_count == 0 || pcnt_unit ||
      glitch_filter_handle || button_handler_added) {
    return OPERATION_RESULT_ERROR;
  }

  operation_result_t result = setup_encoder_button();
  if (result != OPERATION_RESULT_OK) {
    if (release_selector_resources() != OPERATION_RESULT_OK) {
      *handle = &selector_handle;
    }
    return result;
  }

  result = setup_encoder_selector(melody_id_count);
  if (result != OPERATION_RESULT_OK) {
    if (release_selector_resources() != OPERATION_RESULT_OK) {
      *handle = &selector_handle;
    }
    return result;
  }

  melody_list_count = melody_id_count;
  *handle = &selector_handle;
  return OPERATION_RESULT_OK;
}

static operation_result_t setup_encoder_selector(size_t melody_id_count) {
  pcnt_unit_config_t pcnt_config = {
      .high_limit = melody_id_count * 4,
      .low_limit = -melody_id_count * 4,
  };

  OPERATION_RETURN_ON_ERROR(pcnt_new_unit(&pcnt_config, &pcnt_unit), TAG,
                            "Failed to create PCNT unit");

  pcnt_chan_config_t chan_config_0 = {
      .edge_gpio_num = SELECTOR_SIGNAL_A,
      .level_gpio_num = SELECTOR_SIGNAL_B,
  };
  OPERATION_RETURN_ON_ERROR(
      pcnt_new_channel(pcnt_unit, &chan_config_0, &pcnt_channel_0), TAG,
      "Failed to create first PCNT channel");

  OPERATION_RETURN_ON_ERROR(
      pcnt_channel_set_edge_action(pcnt_channel_0,
                                   PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                   PCNT_CHANNEL_EDGE_ACTION_INCREASE),
      TAG, "Failed to configure first PCNT channel edge action");
  OPERATION_RETURN_ON_ERROR(
      pcnt_channel_set_level_action(pcnt_channel_0,
                                    PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                    PCNT_CHANNEL_LEVEL_ACTION_INVERSE),
      TAG, "Failed to configure first PCNT channel level action");

  pcnt_chan_config_t chan_config_1 = {
      .edge_gpio_num = SELECTOR_SIGNAL_B,
      .level_gpio_num = SELECTOR_SIGNAL_A,
  };
  OPERATION_RETURN_ON_ERROR(
      pcnt_new_channel(pcnt_unit, &chan_config_1, &pcnt_channel_1), TAG,
      "Failed to create second PCNT channel");

  OPERATION_RETURN_ON_ERROR(
      pcnt_channel_set_edge_action(pcnt_channel_1,
                                   PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                   PCNT_CHANNEL_EDGE_ACTION_DECREASE),
      TAG, "Failed to configure second PCNT channel edge action");
  OPERATION_RETURN_ON_ERROR(
      pcnt_channel_set_level_action(pcnt_channel_1,
                                    PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                    PCNT_CHANNEL_LEVEL_ACTION_INVERSE),
      TAG, "Failed to configure second PCNT channel level action");

  pcnt_glitch_filter_config_t glitch_filter_config = {
      .max_glitch_ns = 1000,
  };
  OPERATION_RETURN_ON_ERROR(
      pcnt_unit_set_glitch_filter(pcnt_unit, &glitch_filter_config), TAG,
      "Failed to set PCNT glitch filter");
  OPERATION_RETURN_ON_ERROR(
      gpio_set_pull_mode(SELECTOR_SIGNAL_A, GPIO_PULLUP_ONLY), TAG,
      "Failed to set encoder signal A pull mode");
  OPERATION_RETURN_ON_ERROR(
      gpio_set_pull_mode(SELECTOR_SIGNAL_B, GPIO_PULLUP_ONLY), TAG,
      "Failed to set encoder signal B pull mode");
  OPERATION_RETURN_ON_ERROR(pcnt_unit_enable(pcnt_unit), TAG,
                            "Failed to enable PCNT unit");
  pcnt_unit_enabled = true;
  OPERATION_RETURN_ON_ERROR(pcnt_unit_clear_count(pcnt_unit), TAG,
                            "Failed to clear PCNT count");
  OPERATION_RETURN_ON_ERROR(pcnt_unit_start(pcnt_unit), TAG,
                            "Failed to start PCNT unit");
  pcnt_unit_running = true;

  return OPERATION_RESULT_OK;
}

static operation_result_t setup_encoder_button(void) {
  OPERATION_RETURN_ON_ERROR(gpio_reset_pin(SELECTOR_BUTTON), TAG,
                            "Failed to reset selector button pin");
  button_pin_configured = true;
  OPERATION_RETURN_ON_ERROR(
      gpio_set_direction(SELECTOR_BUTTON, GPIO_MODE_INPUT), TAG,
      "Failed to set selector button direction");
  OPERATION_RETURN_ON_ERROR(
      gpio_set_pull_mode(SELECTOR_BUTTON, GPIO_PULLUP_ONLY), TAG,
      "Failed to set selector button pull mode");

  gpio_pin_glitch_filter_config_t filter_config = {
      .clk_src = GLITCH_FILTER_CLK_SRC_DEFAULT,
      .gpio_num = SELECTOR_BUTTON,
  };
  OPERATION_RETURN_ON_ERROR(
      gpio_new_pin_glitch_filter(&filter_config, &glitch_filter_handle), TAG,
      "Failed to create selector glitch filter");
  OPERATION_RETURN_ON_ERROR(gpio_glitch_filter_enable(glitch_filter_handle),
                            TAG, "Failed to enable selector glitch filter");
  button_filter_enabled = true;
  OPERATION_RETURN_ON_ERROR(
      gpio_set_intr_type(SELECTOR_BUTTON, GPIO_INTR_POSEDGE), TAG,
      "Failed to set selector button interrupt type");

  esp_err_t isr_service_err = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
  if (isr_service_err == ESP_OK) {
    gpio_isr_service_owned = true;
  } else if (isr_service_err != ESP_ERR_INVALID_STATE) {
    OPERATION_RETURN_ON_ERROR(isr_service_err, TAG,
                              "Failed to install GPIO ISR service");
  }

  OPERATION_RETURN_ON_ERROR(
      gpio_isr_handler_add(SELECTOR_BUTTON, on_selector_button_clicked,
                           (void*)SELECTOR_BUTTON),
      TAG, "Failed to add selector button handler");
  button_handler_added = true;
  OPERATION_RETURN_ON_ERROR(gpio_intr_enable(SELECTOR_BUTTON), TAG,
                            "Failed to enable selector button interrupt");
  button_interrupt_enabled = true;

  return OPERATION_RESULT_OK;
}

operation_result_t release_selector(selector_handle_t handle) {
  if (handle != &selector_handle) {
    return OPERATION_RESULT_ERROR;
  }

  return release_selector_resources();
}

selection_result_t get_selector_state(selector_handle_t handle) {
  selection_result_t result = {.state = OPTION_NOT_SELECTED};

  if (handle != &selector_handle || !pcnt_unit) {
    return result;
  }

  int encoder_value = 0;
  esp_err_t err = pcnt_unit_get_count(pcnt_unit, &encoder_value);
  int option_index = fabs(encoder_value * 0.25);
  result.preselected_option = option_index;

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read PCNT count: %s", esp_err_to_name(err));
    result.state = OPTION_ERROR;
    return result;
  }

  if (is_selected) {
    ESP_LOGI(TAG, "Selected option: %d", option_index);
    result.state = OPTION_SELECTED;
    result.selected_option = option_index;
  }

  return result;
}

operation_result_t mark_selection_processed(selector_handle_t handle) {
  if (handle != &selector_handle) {
    return OPERATION_RESULT_ERROR;
  }

  is_selected = false;
  return OPERATION_RESULT_OK;
}

IRAM_ATTR void on_selector_button_clicked(void* arg) {
  (void)arg;
  is_selected = true;
}

static operation_result_t release_selector_resources(void) {
  bool success = true;

  if (button_interrupt_enabled) {
    esp_err_t err = gpio_intr_disable(SELECTOR_BUTTON);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to disable selector button interrupt: %s",
               esp_err_to_name(err));
      success = false;
    } else {
      button_interrupt_enabled = false;
    }
  }

  if (button_handler_added && !button_interrupt_enabled) {
    esp_err_t err = gpio_isr_handler_remove(SELECTOR_BUTTON);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to remove selector button handler: %s",
               esp_err_to_name(err));
      success = false;
    } else {
      button_handler_added = false;
    }
  }

  if (gpio_isr_service_owned && !button_handler_added) {
    gpio_uninstall_isr_service();
    gpio_isr_service_owned = false;
  }

  if (glitch_filter_handle && button_filter_enabled) {
    esp_err_t err = gpio_glitch_filter_disable(glitch_filter_handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to disable selector glitch filter: %s",
               esp_err_to_name(err));
      success = false;
    } else {
      button_filter_enabled = false;
    }
  }
  if (glitch_filter_handle && !button_filter_enabled) {
    esp_err_t err = gpio_del_glitch_filter(glitch_filter_handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to delete selector glitch filter: %s",
               esp_err_to_name(err));
      success = false;
    } else {
      glitch_filter_handle = NULL;
    }
  }

  bool pcnt_unit_stopped = !pcnt_unit_running;
  if (pcnt_unit && pcnt_unit_running) {
    esp_err_t err = pcnt_unit_stop(pcnt_unit);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to stop PCNT unit: %s", esp_err_to_name(err));
      success = false;
    } else {
      pcnt_unit_running = false;
      pcnt_unit_stopped = true;
    }
  }

  bool pcnt_unit_disabled = !pcnt_unit_enabled;
  if (pcnt_unit && pcnt_unit_stopped && pcnt_unit_enabled) {
    esp_err_t err = pcnt_unit_disable(pcnt_unit);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to disable PCNT unit: %s", esp_err_to_name(err));
      success = false;
    } else {
      pcnt_unit_enabled = false;
      pcnt_unit_disabled = true;
    }
  }

  if (pcnt_unit && pcnt_unit_disabled) {
    if (pcnt_channel_0) {
      esp_err_t err = pcnt_del_channel(pcnt_channel_0);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete first PCNT channel: %s",
                 esp_err_to_name(err));
        success = false;
      } else {
        pcnt_channel_0 = NULL;
      }
    }
    if (pcnt_channel_1) {
      esp_err_t err = pcnt_del_channel(pcnt_channel_1);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete second PCNT channel: %s",
                 esp_err_to_name(err));
        success = false;
      } else {
        pcnt_channel_1 = NULL;
      }
    }
    if (!pcnt_channel_0 && !pcnt_channel_1) {
      esp_err_t err = pcnt_del_unit(pcnt_unit);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete PCNT unit: %s", esp_err_to_name(err));
        success = false;
      } else {
        pcnt_unit = NULL;
      }
    }
  }

  if (button_pin_configured && !button_handler_added && !glitch_filter_handle) {
    esp_err_t err = gpio_reset_pin(SELECTOR_BUTTON);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to reset selector button pin: %s",
               esp_err_to_name(err));
      success = false;
    } else {
      button_pin_configured = false;
    }
  }

  if (!pcnt_unit) {
    esp_err_t err = gpio_reset_pin(SELECTOR_SIGNAL_A);
    if (err == ESP_OK) {
      err = gpio_reset_pin(SELECTOR_SIGNAL_B);
    }
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to reset selector encoder pins: %s",
               esp_err_to_name(err));
      success = false;
    }
  }

  if (!pcnt_unit && !button_handler_added && !glitch_filter_handle) {
    melody_list_count = 0;
    is_selected = false;
  }

  return success ? OPERATION_RESULT_OK : OPERATION_RESULT_ERROR;
}
