#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct display_manager display_manager_t;
typedef display_manager_t *display_handle_t;

esp_err_t display_init(display_handle_t *out_handle);
void display_deinit(display_handle_t handle);
esp_err_t display_clear(display_handle_t handle);
esp_err_t display_set_inverted(display_handle_t handle, bool inverted);
esp_err_t display_show_status(display_handle_t handle, const char *status);
esp_err_t display_show_notification(display_handle_t handle, const char *app, const char *title, const char *message);

#ifdef __cplusplus
}
#endif
