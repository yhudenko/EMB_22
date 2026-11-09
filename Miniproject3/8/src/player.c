#include "player.h"

#include "driver/gptimer.h"
#include "driver/ledc.h"
#include "errors.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "melody.h"

#define BUZZER_PIN 16
#define PERFORMANCE_DUTY ((1 << LEDC_TIMER_13_BIT) / 2)
#define SILENCE_DUTY 0
#define IDLE_FREQUENCY 5000
#define MICROSECONDS_IN_MILLISECOND 1000
#define ONE_MHZ 1000000

static const char* TAG = "PLAYER";

struct melody_context {
  melody_handle_t melody;
  player_handle_t player;
};

typedef struct melody_context melody_context_t;

static int player_handle = 1;
static gptimer_handle_t gptimer = NULL;
static gptimer_alarm_config_t gptimer_alarm_conf;
static melody_context_t melody_ctx;
static bool ledc_timer_configured;
static bool ledc_channel_configured;
static volatile bool gptimer_enabled;
static volatile bool gptimer_running;

static bool on_note_completed(struct gptimer_t* timer,
                              const gptimer_alarm_event_data_t* edata,
                              void* user_ctx);
static operation_result_t play_next_note(melody_handle_t melody);
static operation_result_t release_player_resources(void);

static operation_result_t configure_player_hardware(void);
static void reset_melody_context(void);
static void disable_timer_and_reset_melody_context(void);

operation_result_t initialize_player(player_handle_t* handle) {
  if (!handle || *handle || gptimer || ledc_timer_configured ||
      ledc_channel_configured) {
    return OPERATION_RESULT_ERROR;
  }

  operation_result_t result = configure_player_hardware();
  if (result != OPERATION_RESULT_OK) {
    if (release_player_resources() != OPERATION_RESULT_OK) {
      *handle = &player_handle;
    }
    return result;
  }

  *handle = &player_handle;
  return OPERATION_RESULT_OK;
}

static operation_result_t configure_player_hardware(void) {
  ledc_timer_config_t timer_conf = {
      .duty_resolution = LEDC_TIMER_13_BIT,
      .freq_hz = IDLE_FREQUENCY,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .timer_num = LEDC_TIMER_0,
      .clk_cfg = LEDC_AUTO_CLK,
  };

  OPERATION_RETURN_ON_ERROR(ledc_timer_config(&timer_conf), TAG,
                            "Failed to configure LEDC timer");
  ledc_timer_configured = true;

  ledc_channel_config_t channel_conf = {
      .channel = LEDC_CHANNEL_0,
      .duty = SILENCE_DUTY,
      .gpio_num = BUZZER_PIN,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .hpoint = 0,
      .timer_sel = LEDC_TIMER_0,
  };

  OPERATION_RETURN_ON_ERROR(ledc_channel_config(&channel_conf), TAG,
                            "Failed to configure LEDC channel");
  ledc_channel_configured = true;

  gptimer_config_t gptimer_conf = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = ONE_MHZ,
  };

  OPERATION_RETURN_ON_ERROR(gptimer_new_timer(&gptimer_conf, &gptimer), TAG,
                            "Failed to create GPTimer");

  gptimer_event_callbacks_t gptimer_callbacks = {
      .on_alarm = on_note_completed,
  };
  OPERATION_RETURN_ON_ERROR(
      gptimer_register_event_callbacks(gptimer, &gptimer_callbacks,
                                       &melody_ctx),
      TAG, "Failed to register GPTimer callback");

  return OPERATION_RESULT_OK;
}

operation_result_t play_melody(player_handle_t player, melody_handle_t melody) {
  if (player != &player_handle || !melody || !gptimer ||
      melody_ctx.player != NULL) {
    return OPERATION_RESULT_ERROR;
  }

  melody_ctx.player = player;
  melody_ctx.melody = melody;
  OPERATION_RETURN_ON_OPERATION_ERROR_WITH_CALLBACK(
      play_next_note(melody), reset_melody_context, TAG,
      "Failed to configure first melody note");

  OPERATION_RETURN_ON_ERROR_WITH_CALLBACK(
      gptimer_enable(gptimer), reset_melody_context, TAG,
      "Failed to enable GPTimer");
  gptimer_enabled = true;

  OPERATION_RETURN_ON_ERROR_WITH_CALLBACK(
      gptimer_start(gptimer), disable_timer_and_reset_melody_context, TAG,
      "Failed to start GPTimer");
  gptimer_running = true;

  return OPERATION_RESULT_OK;
}

static void reset_melody_context(void) {
  melody_ctx.player = NULL;
  melody_ctx.melody = NULL;
}

static void disable_timer_and_reset_melody_context(void) {
  esp_err_t disable_err = gptimer_disable(gptimer);
  if (disable_err == ESP_OK) {
    gptimer_enabled = false;
  } else {
    ESP_LOGE(TAG, "Failed to disable GPTimer after start failure: %s",
             esp_err_to_name(disable_err));
  }
  reset_melody_context();
}

operation_result_t stop_melody(player_handle_t player, melody_handle_t melody) {
  if (player != &player_handle || !melody || !gptimer ||
      (melody_ctx.player != NULL &&
       (melody_ctx.player != player || melody_ctx.melody != melody))) {
    return OPERATION_RESULT_ERROR;
  }

  bool success = true;
  if (gptimer_running) {
    esp_err_t err = gptimer_stop(gptimer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to stop GPTimer: %s", esp_err_to_name(err));
      success = false;
    } else {
      gptimer_running = false;
    }
  }

  if (gptimer_enabled && !gptimer_running) {
    esp_err_t err = gptimer_disable(gptimer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to disable GPTimer: %s", esp_err_to_name(err));
      success = false;
    } else {
      gptimer_enabled = false;
    }
  }

  esp_err_t err =
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, SILENCE_DUTY);
  if (err == ESP_OK) {
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to silence buzzer: %s", esp_err_to_name(err));
    success = false;
  }

  if (success) {
    melody_ctx.player = NULL;
    melody_ctx.melody = NULL;
  }
  return success ? OPERATION_RESULT_OK : OPERATION_RESULT_ERROR;
}

operation_result_t release_player(player_handle_t player) {
  if (player != &player_handle) {
    return OPERATION_RESULT_ERROR;
  }
  return release_player_resources();
}

bool is_melody_playing(player_handle_t player, melody_handle_t melody) {
  if (player != &player_handle || !melody) {
    return false;
  }

  return melody_ctx.player == player && melody_ctx.melody == melody &&
         gptimer_running;
}

IRAM_ATTR static operation_result_t setup_to_play_next_note(
    uint32_t* frequency, uint32_t* duration) {
  if (!frequency || !duration) {
    return OPERATION_RESULT_ERROR;
  }

  uint32_t target_frequency = *frequency == 0 ? IDLE_FREQUENCY : *frequency;
  uint32_t target_duty = *frequency == 0 ? SILENCE_DUTY : PERFORMANCE_DUTY;

  OPERATION_RETURN_ON_ERROR(
      ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, target_frequency), TAG,
      "Failed to set buzzer frequency");

  OPERATION_RETURN_ON_ERROR(
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, target_duty), TAG,
      "Failed to set buzzer duty");

  OPERATION_RETURN_ON_ERROR(
      ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0), TAG,
      "Failed to update buzzer duty");

  gptimer_alarm_conf.alarm_count =
      *duration * MICROSECONDS_IN_MILLISECOND;
  gptimer_alarm_conf.flags.auto_reload_on_alarm = true;
  OPERATION_RETURN_ON_ERROR(
      gptimer_set_alarm_action(gptimer, &gptimer_alarm_conf), TAG,
      "Failed to set note timer alarm");
  return OPERATION_RESULT_OK;
}

IRAM_ATTR static operation_result_t play_next_note(melody_handle_t melody) {
  uint32_t frequency, duration;
  OPERATION_PROPAGATE_OPERATION_ERROR(
      get_next_note_params(melody, &frequency, &duration));

  return setup_to_play_next_note(&frequency, &duration);
}

IRAM_ATTR static bool on_note_completed(struct gptimer_t* timer,
                                        const gptimer_alarm_event_data_t* edata,
                                        void* user_ctx) {
  (void)timer;
  (void)edata;
  melody_context_t* context = (melody_context_t*)user_ctx;
  if (play_next_note(context->melody) != OPERATION_RESULT_OK) {
    esp_err_t err = gptimer_stop(gptimer);
    if (err == ESP_OK) {
      gptimer_running = false;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, SILENCE_DUTY);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  }

  return false;
}

static operation_result_t release_player_resources(void) {
  bool success = true;

  if (gptimer && gptimer_running) {
    esp_err_t err = gptimer_stop(gptimer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to stop GPTimer during release: %s",
               esp_err_to_name(err));
      success = false;
    } else {
      gptimer_running = false;
    }
  }

  if (gptimer && gptimer_enabled && !gptimer_running) {
    esp_err_t err = gptimer_disable(gptimer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to disable GPTimer during release: %s",
               esp_err_to_name(err));
      success = false;
    } else {
      gptimer_enabled = false;
    }
  }

  if (gptimer && !gptimer_enabled && !gptimer_running) {
    esp_err_t err = gptimer_del_timer(gptimer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to delete GPTimer: %s", esp_err_to_name(err));
      success = false;
    } else {
      gptimer = NULL;
    }
  }

  if (gptimer) {
    return OPERATION_RESULT_ERROR;
  }

  bool ledc_channel_released = !ledc_channel_configured;
  if (ledc_channel_configured) {
    esp_err_t err = ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to stop LEDC channel: %s",
               esp_err_to_name(err));
      success = false;
    } else {
      ledc_channel_configured = false;
      ledc_channel_released = true;
    }
  }

  if (ledc_timer_configured && ledc_channel_released) {
    esp_err_t err = ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0);
    if (err == ESP_OK) {
      ledc_timer_config_t timer_conf = {
          .speed_mode = LEDC_LOW_SPEED_MODE,
          .timer_num = LEDC_TIMER_0,
          .deconfigure = true,
      };
      err = ledc_timer_config(&timer_conf);
    }
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to deconfigure LEDC timer: %s",
               esp_err_to_name(err));
      success = false;
    } else {
      ledc_timer_configured = false;
    }
  }

  if (gptimer == NULL && !ledc_timer_configured &&
      !ledc_channel_configured) {
    melody_ctx.player = NULL;
    melody_ctx.melody = NULL;
  }

  return success ? OPERATION_RESULT_OK : OPERATION_RESULT_ERROR;
}
