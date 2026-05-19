#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POWER_SOURCE_BATTERY = 0,
    POWER_SOURCE_CHARGING,
    POWER_SOURCE_CHARGED,
    POWER_SOURCE_UNKNOWN,
} power_source_t;

typedef struct {
    uint16_t battery_mv;
    uint8_t  battery_pct;
    power_source_t source;
    bool low_battery;
    bool critical_battery;
} power_state_t;

typedef void (*power_state_cb_t)(const power_state_t *state, void *user_ctx);

typedef struct {
    power_state_cb_t state_cb;
    void *user_ctx;
} power_manager_config_t;

esp_err_t power_manager_init(const power_manager_config_t *config);
void power_manager_get_state(power_state_t *out_state);
const char *power_source_to_string(power_source_t source);

#ifdef __cplusplus
}
#endif
