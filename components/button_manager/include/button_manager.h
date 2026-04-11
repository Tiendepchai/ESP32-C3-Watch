#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BUTTON_MANAGER_EVENT_A_SHORT = 0,
    BUTTON_MANAGER_EVENT_B_SHORT,
    BUTTON_MANAGER_EVENT_A_LONG,
    BUTTON_MANAGER_EVENT_B_LONG,
    BUTTON_MANAGER_EVENT_AB_LONG,
} button_manager_event_t;

typedef struct {
    void (*event_cb)(button_manager_event_t event, void *user_ctx);
    void *user_ctx;
} button_manager_config_t;

esp_err_t button_manager_init(const button_manager_config_t *config);
void button_manager_deinit(void);

#ifdef __cplusplus
}
#endif
