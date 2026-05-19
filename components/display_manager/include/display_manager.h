#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "text_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct display_manager display_manager_t;
typedef display_manager_t *display_handle_t;

esp_err_t display_init(display_handle_t *out_handle);
void display_deinit(display_handle_t handle);
esp_err_t display_clear(display_handle_t handle);
esp_err_t display_set_inverted(display_handle_t handle, bool inverted);
esp_err_t display_set_active(display_handle_t handle, bool active);
esp_err_t display_show_status(display_handle_t handle, const char *status);
esp_err_t display_show_notification(display_handle_t handle, const char *app, const char *title,
                                    const char *message, nav_icon_t icon);
esp_err_t display_show_navigation(display_handle_t handle, const char *source, const char *title,
                                  const char *instruction, const char *distance, const char *eta,
                                  nav_icon_t icon);
esp_err_t display_show_watchface(display_handle_t handle, const char *time_str, const char *date_str,
                                 const char *battery_str, const char *source_str);

#ifdef __cplusplus
}
#endif
