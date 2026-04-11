#include "button_manager.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "board_config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
    bool stable_pressed;
    bool raw_pressed;
    bool consumed;
    uint32_t raw_change_ms;
    uint32_t pressed_since_ms;
} button_state_t;

typedef struct {
    button_manager_config_t cfg;
    TaskHandle_t task;
    button_state_t button_a;
    button_state_t button_b;
    uint32_t combo_since_ms;
    bool combo_consumed;
    bool initialized;
} button_manager_ctx_t;

static const char *TAG = BOARD_TAG_BUTTON;
static button_manager_ctx_t s_button;

static uint32_t button_manager_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static bool button_manager_gpio_pressed(gpio_num_t gpio)
{
    if (gpio < 0) {
        return false;
    }
    return gpio_get_level(gpio) == 0;
}

static bool button_manager_group_pressed(gpio_num_t primary, gpio_num_t secondary)
{
    return button_manager_gpio_pressed(primary) || button_manager_gpio_pressed(secondary);
}

static void button_manager_format_pins(char *buf, size_t buf_len, gpio_num_t primary, gpio_num_t secondary)
{
    if (buf == NULL || buf_len == 0U) {
        return;
    }

    if (primary < 0 && secondary < 0) {
        strlcpy(buf, "disabled", buf_len);
        return;
    }

    if (secondary >= 0) {
        snprintf(buf, buf_len, "GPIO%d/GPIO%d", primary, secondary);
    } else {
        snprintf(buf, buf_len, "GPIO%d", primary);
    }
}

static void button_manager_emit(button_manager_event_t event)
{
    if (s_button.cfg.event_cb != NULL) {
        s_button.cfg.event_cb(event, s_button.cfg.user_ctx);
    }
}

static void button_manager_update_state(button_state_t *state, bool raw_pressed, uint32_t now_ms)
{
    if (state == NULL) {
        return;
    }

    if (raw_pressed != state->raw_pressed) {
        state->raw_pressed = raw_pressed;
        state->raw_change_ms = now_ms;
    }

    if ((now_ms - state->raw_change_ms) < BOARD_BUTTON_DEBOUNCE_MS ||
        state->stable_pressed == state->raw_pressed) {
        return;
    }

    state->stable_pressed = state->raw_pressed;
    if (state->stable_pressed) {
        state->pressed_since_ms = now_ms;
        state->consumed = false;
    }
}

static void button_manager_handle_release(button_state_t *state, button_manager_event_t short_event)
{
    if (state == NULL) {
        return;
    }

    if (!state->consumed && state->pressed_since_ms != 0U) {
        button_manager_emit(short_event);
    }

    state->pressed_since_ms = 0U;
    state->consumed = false;
}

static void button_manager_task(void *arg)
{
    (void)arg;

    while (true) {
        uint32_t now_ms = button_manager_now_ms();
        bool raw_a = button_manager_group_pressed(BOARD_BUTTON_A_GPIO_PRIMARY, BOARD_BUTTON_A_GPIO_SECONDARY);
        bool raw_b = button_manager_group_pressed(BOARD_BUTTON_B_GPIO_PRIMARY, BOARD_BUTTON_B_GPIO_SECONDARY);
        bool was_a_pressed = s_button.button_a.stable_pressed;
        bool was_b_pressed = s_button.button_b.stable_pressed;
        bool both_pressed;

        button_manager_update_state(&s_button.button_a, raw_a, now_ms);
        button_manager_update_state(&s_button.button_b, raw_b, now_ms);

        both_pressed = s_button.button_a.stable_pressed && s_button.button_b.stable_pressed;

        if (both_pressed) {
            if (s_button.combo_since_ms == 0U) {
                s_button.combo_since_ms = now_ms;
                s_button.combo_consumed = false;
            }

            if (!s_button.combo_consumed &&
                !s_button.button_a.consumed &&
                !s_button.button_b.consumed &&
                (now_ms - s_button.combo_since_ms) >= BOARD_BUTTON_AB_LONG_PRESS_MS) {
                s_button.combo_consumed = true;
                s_button.button_a.consumed = true;
                s_button.button_b.consumed = true;
                button_manager_emit(BUTTON_MANAGER_EVENT_AB_LONG);
            }
        } else {
            s_button.combo_since_ms = 0U;
            s_button.combo_consumed = false;
        }

        if (s_button.button_a.stable_pressed &&
            !s_button.button_b.stable_pressed &&
            !s_button.button_a.consumed &&
            s_button.button_a.pressed_since_ms != 0U &&
            (now_ms - s_button.button_a.pressed_since_ms) >= BOARD_BUTTON_LONG_PRESS_MS) {
            s_button.button_a.consumed = true;
            button_manager_emit(BUTTON_MANAGER_EVENT_A_LONG);
        }

        if (s_button.button_b.stable_pressed &&
            !s_button.button_a.stable_pressed &&
            !s_button.button_b.consumed &&
            s_button.button_b.pressed_since_ms != 0U &&
            (now_ms - s_button.button_b.pressed_since_ms) >= BOARD_BUTTON_LONG_PRESS_MS) {
            s_button.button_b.consumed = true;
            button_manager_emit(BUTTON_MANAGER_EVENT_B_LONG);
        }

        if (was_a_pressed && !s_button.button_a.stable_pressed) {
            button_manager_handle_release(&s_button.button_a, BUTTON_MANAGER_EVENT_A_SHORT);
        }
        if (was_b_pressed && !s_button.button_b.stable_pressed) {
            button_manager_handle_release(&s_button.button_b, BUTTON_MANAGER_EVENT_B_SHORT);
        }

        vTaskDelay(pdMS_TO_TICKS(BOARD_BUTTON_POLL_MS));
    }
}

esp_err_t button_manager_init(const button_manager_config_t *config)
{
    const gpio_num_t gpios[] = {
        BOARD_BUTTON_A_GPIO_PRIMARY,
        BOARD_BUTTON_A_GPIO_SECONDARY,
        BOARD_BUTTON_B_GPIO_PRIMARY,
        BOARD_BUTTON_B_GPIO_SECONDARY,
    };
    gpio_config_t io_conf = {0};
    char button_a_pins[24];
    char button_b_pins[24];

    ESP_RETURN_ON_FALSE(config != NULL && config->event_cb != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid config");

    if (s_button.initialized) {
        return ESP_OK;
    }

    memset(&s_button, 0, sizeof(s_button));
    s_button.cfg = *config;

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    for (size_t i = 0; i < sizeof(gpios) / sizeof(gpios[0]); ++i) {
        if (gpios[i] < 0) {
            continue;
        }
        io_conf.pin_bit_mask = 1ULL << gpios[i];
        ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "gpio_config failed for GPIO%d", gpios[i]);
    }

    if (BOARD_BUTTON_A_GPIO_PRIMARY == GPIO_NUM_21 ||
        BOARD_BUTTON_A_GPIO_SECONDARY == GPIO_NUM_21 ||
        BOARD_BUTTON_B_GPIO_PRIMARY == GPIO_NUM_21 ||
        BOARD_BUTTON_B_GPIO_SECONDARY == GPIO_NUM_21) {
        ESP_LOGW(TAG, "GPIO21 may conflict with the default console UART on ESP32-C3");
    }

    BaseType_t task_ok = xTaskCreate(button_manager_task, "button_mgr", 3072, NULL, 4, &s_button.task);
    ESP_RETURN_ON_FALSE(task_ok == pdPASS, ESP_FAIL, TAG, "failed to create button task");

    s_button.initialized = true;
    button_manager_format_pins(button_a_pins, sizeof(button_a_pins),
                               BOARD_BUTTON_A_GPIO_PRIMARY, BOARD_BUTTON_A_GPIO_SECONDARY);
    button_manager_format_pins(button_b_pins, sizeof(button_b_pins),
                               BOARD_BUTTON_B_GPIO_PRIMARY, BOARD_BUTTON_B_GPIO_SECONDARY);
    ESP_LOGI(TAG, "buttons initialized: A(%s) B(%s)", button_a_pins, button_b_pins);
    return ESP_OK;
}

void button_manager_deinit(void)
{
    if (!s_button.initialized) {
        return;
    }

    if (s_button.task != NULL) {
        vTaskDelete(s_button.task);
    }
    memset(&s_button, 0, sizeof(s_button));
}
