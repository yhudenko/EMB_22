#include "display.h"

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <ssd1306.h>
#include <stdio.h>

#define DISPLAY_HANDLE_VALUE 1
#define DISPLAY_I2C_PORT I2C_NUM_0
#define DISPLAY_I2C_SDA_GPIO GPIO_NUM_11
#define DISPLAY_I2C_SCL_GPIO GPIO_NUM_12
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DISPLAY_I2C_PROBE_TIMEOUT_MS 100
#define DISPLAY_I2C_GLITCH_IGNORE_COUNT 7
#define DISPLAY_I2C_ADDRESS 0x3C
#define DISPLAY_TEXT_BUFFER_SIZE 128
#define DISPLAY_TEXT_X 15
#define DISPLAY_TEXT_Y 0
#define DISPLAY_ROW_HEIGHT 9
#define DISPLAY_MARKER_X 0
#define DISPLAY_MARKER_SIZE 6
#define DISPLAY_MARKER_RADIUS 3

static const char* TAG = "DISPLAY";

static display_handle_t active_display_handle;
static i2c_master_bus_handle_t i2c_bus_handle;
static ssd1306_handle_t ssd1306_handle;
static char text_buffer[DISPLAY_TEXT_BUFFER_SIZE];
static int selected_option = DISPLAY_NOT_SELECTED_OPTION;

static void fill_text_buffer(char* text_buffer, size_t buffer_size,
                             const display_update_data_t* update_data);
static operation_result_t release_hardware(void);
static void cleanup_failed_initialization(void);

operation_result_t initialize_display(display_handle_t* handle) {
  if (!handle || active_display_handle || i2c_bus_handle || ssd1306_handle) {
    ESP_LOGE(TAG, "Display is already initialized or handle is null");
    return OPERATION_RESULT_ERROR;
  }

  const i2c_master_bus_config_t bus_config = {
      .i2c_port = DISPLAY_I2C_PORT,
      .sda_io_num = DISPLAY_I2C_SDA_GPIO,
      .scl_io_num = DISPLAY_I2C_SCL_GPIO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = DISPLAY_I2C_GLITCH_IGNORE_COUNT,
      .flags.enable_internal_pullup = true,
  };
  OPERATION_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &i2c_bus_handle),
                            TAG, "Failed to initialize I2C bus");

  const ssd1306_config_t config = {
      .width = DISPLAY_WIDTH,
      .height = DISPLAY_HEIGHT,
      .fb = NULL,
      .fb_len = 0,
      .iface.i2c =
          {
              .port = DISPLAY_I2C_PORT,
              .rst_gpio = GPIO_NUM_NC,
              .addr = DISPLAY_I2C_ADDRESS,
          },
  };
  OPERATION_RETURN_ON_ERROR_WITH_CALLBACK(
      ssd1306_new_i2c(&config, &ssd1306_handle), cleanup_failed_initialization,
      TAG, "Failed to initialize SSD1306");

  active_display_handle = DISPLAY_HANDLE_VALUE;
  *handle = active_display_handle;
  return OPERATION_RESULT_OK;
}

operation_result_t release_display(display_handle_t handle) {
  if (handle != active_display_handle || !active_display_handle) {
    ESP_LOGE(TAG, "Invalid display handle");
    return OPERATION_RESULT_ERROR;
  }

  OPERATION_PROPAGATE_OPERATION_ERROR(release_hardware());

  active_display_handle = 0;
  return OPERATION_RESULT_OK;
}

operation_result_t update_display(display_handle_t handle,
                                  const display_update_data_t* update_data) {
  if (handle != active_display_handle || !ssd1306_handle || !update_data ||
      (update_data->option_count > 0 && !update_data->options)) {
    ESP_LOGE(TAG, "Invalid display handle");
    return OPERATION_RESULT_ERROR;
  }

  fill_text_buffer(text_buffer, sizeof(text_buffer), update_data);

  OPERATION_RETURN_ON_ERROR(ssd1306_clear(ssd1306_handle), TAG,
                            "Failed to clear display");

  if (update_data->selected_option >= 0) {
    selected_option = update_data->selected_option;
  }
  const bool has_selection =
      selected_option >= 0 &&
      (size_t)selected_option < update_data->option_count;

  OPERATION_RETURN_ON_ERROR(
      ssd1306_draw_text(ssd1306_handle, DISPLAY_TEXT_X, DISPLAY_TEXT_Y,
                        text_buffer, true),
      TAG, "Failed to draw display text");

  if (has_selection) {
    OPERATION_RETURN_ON_ERROR(
        ssd1306_draw_rect(ssd1306_handle, DISPLAY_MARKER_X,
                          DISPLAY_ROW_HEIGHT * selected_option,
                          DISPLAY_MARKER_SIZE, DISPLAY_MARKER_SIZE, true),
        TAG, "Failed to draw selection marker");
  }

  if (update_data->preselected_option >= 0 &&
      (size_t)update_data->preselected_option < update_data->option_count &&
      !(has_selection && update_data->preselected_option == selected_option)) {
    int y = DISPLAY_ROW_HEIGHT * update_data->preselected_option;
    OPERATION_RETURN_ON_ERROR(
        ssd1306_draw_circle(ssd1306_handle, DISPLAY_MARKER_RADIUS,
                            DISPLAY_MARKER_RADIUS + y, DISPLAY_MARKER_RADIUS,
                            false),
        TAG, "Failed to draw preselection marker");
  }

  OPERATION_RETURN_ON_ERROR(ssd1306_display(ssd1306_handle), TAG,
                            "Failed to update display");

  return OPERATION_RESULT_OK;
}

static operation_result_t release_hardware(void) {
  if (ssd1306_handle) {
    OPERATION_RETURN_ON_ERROR(ssd1306_del(ssd1306_handle), TAG,
                              "Failed to release SSD1306");
    ssd1306_handle = NULL;
  }

  if (i2c_bus_handle) {
    OPERATION_RETURN_ON_ERROR(i2c_del_master_bus(i2c_bus_handle), TAG,
                              "Failed to release I2C bus");
    i2c_bus_handle = NULL;
  }

  return OPERATION_RESULT_OK;
}

static void cleanup_failed_initialization(void) { release_hardware(); }

static void fill_text_buffer(char* text_buffer, size_t buffer_size,
                             const display_update_data_t* update_data) {
  if (!text_buffer || buffer_size == 0) {
    return;
  }

  text_buffer[0] = '\0';
  if (!update_data || !update_data->options) {
    return;
  }

  size_t used = 0;
  for (size_t i = 0; i < update_data->option_count; i++) {
    int written = snprintf(text_buffer + used, buffer_size - used, "%s%s",
                           i > 0 ? "\n" : "", update_data->options[i]);
    if (written < 0 || (size_t)written >= buffer_size - used) {
      break;
    }
    used += (size_t)written;
  }
}
