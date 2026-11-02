#include <stdio.h>
#include "driver/gptimer.h"
#include "esp_attr.h"
#include "esp_rom_sys.h"
#include "seven_segment_config.h"
#include "traffic_light_config.h"

traffic_phase_t current_phase = PHASE_RED;
int seconds_left = RED_PHASE_DURATION_SEC;
int display_value = RED_PHASE_DURATION_SEC;
bool blink_state = true;

volatile uint32_t system_ticks = 0; 

void display_digit(int digit, int display_num) {
    gpio_set_level(DIG_1, 1);
    gpio_set_level(DIG_2, 1);

    uint8_t pattern = num_patterns[digit];
    for (int i = 0; i < 8; i++) {
        gpio_set_level(segment_pins[i], (pattern >> i) & 0x01);
    }
    gpio_set_level(digit_pins[display_num], 0);
}

void set_traffic_lights(bool red, bool yellow, bool green) {
    gpio_set_level(LED_RED, red);
    gpio_set_level(LED_YELLOW, yellow);
    gpio_set_level(LED_GREEN, green);
}

void process_traffic_logic(void) {
    seconds_left--;

    switch (current_phase) {
        case PHASE_RED:
            display_value = seconds_left;
            if (seconds_left <= RED_YELLOW_PHASE_DURATION_SEC) {
                current_phase = PHASE_RED_YELLOW;
            }
            break;
        case PHASE_RED_YELLOW:
            display_value = seconds_left;
            if (seconds_left <= 0) {
                current_phase = PHASE_GREEN;
                seconds_left = GREEN_PHASE_DURATION_SEC;
            }
            break;
        case PHASE_GREEN:
            display_value = seconds_left;
            if (seconds_left <= GREEN_BLINK_DURATION_SEC) {
                current_phase = PHASE_GREEN_BLINK;
            }
            break;
        case PHASE_GREEN_BLINK:
            display_value = seconds_left;
            if (seconds_left <= 0) {
                current_phase = PHASE_YELLOW;
                seconds_left = YELLOW_PHASE_DURATION_SEC;
            }
            break;
        case PHASE_YELLOW:
            display_value = seconds_left;
            if (seconds_left <= 0) {
                current_phase = PHASE_RED;
                seconds_left = RED_PHASE_DURATION_SEC;
            }
            break;
    }
}

static bool IRAM_ATTR timer_on_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data) {
    system_ticks++; 
    return false;
}

void app_main(void) {
    //setup
    uint64_t pin_mask = (1ULL << LED_RED) | (1ULL << LED_YELLOW) | (1ULL << LED_GREEN);
    for (int i = 0; i < 8; i++)
        pin_mask |= (1ULL << segment_pins[i]);
    for (int i = 0; i < 2; i++)
        pin_mask |= (1ULL << digit_pins[i]);

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = pin_mask,
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);

    for (int i = 0; i < 8; i++)
        gpio_set_level(segment_pins[i], 1);
    for (int i = 0; i < 2; i++)
        gpio_set_level(digit_pins[i], 1);
    set_traffic_lights(0, 0, 0);


    gptimer_handle_t timer = NULL;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Джерело тактування за замовчуванням (зазвичай APB 80 МГц)
        .direction = GPTIMER_COUNT_UP,      // Рахуємо вгору
        .resolution_hz = 1000000,           // Частота таймера 1 МГц (1 тик = 1 мікросекунда)
    };
    gptimer_new_timer(&timer_config, &timer);

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 1000,                // Спрацьовувати кожні 1000 мікросекунд (1 мс)
        .flags.auto_reload_on_alarm = true, // Автоматичне перезавантаження таймера після спрацьовування
    };
    gptimer_set_alarm_action(timer, &alarm_config);

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_on_alarm_cb,
    };
    gptimer_register_event_callbacks(timer, &cbs, NULL);

    gptimer_enable(timer);
    gptimer_start(timer);

    uint32_t last_logic_tick = 0;
    uint32_t last_blink_tick = 0;
    uint32_t last_display_tick = 0;
    
    int current_digit = 0;

    while (1) {
        uint32_t current_tick = system_ticks;
        
        if (current_tick - last_logic_tick >= 1000) {
            last_logic_tick = current_tick;
            process_traffic_logic();
        }

        if (current_tick - last_blink_tick >= 500) {
            last_blink_tick = current_tick;
            blink_state = !blink_state;
        }

        switch (current_phase) {
            case PHASE_RED:          set_traffic_lights(1, 0, 0); break;
            case PHASE_RED_YELLOW:   set_traffic_lights(1, 1, 0); break;
            case PHASE_GREEN:        set_traffic_lights(0, 0, 1); break;
            case PHASE_GREEN_BLINK:  set_traffic_lights(0, 0, blink_state); break;
            case PHASE_YELLOW:       set_traffic_lights(0, 1, 0); break;
        }

        if (current_tick - last_display_tick >= 5) {
            last_display_tick = current_tick;

            int tens = display_value / 10; // *0.1f;
            int ones = display_value % 10;

            if (current_phase == PHASE_GREEN_BLINK && !blink_state) {
                gpio_set_level(DIG_1, 1);
                gpio_set_level(DIG_2, 1);
            } else {
                if (current_digit == 0) {
                    display_digit(tens, 0);
                    current_digit = 1;
                } else {
                    display_digit(ones, 1);
                    current_digit = 0;
                }
            }
        }
        
        esp_rom_delay_us(10); 
    }
}