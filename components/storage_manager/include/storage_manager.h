#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "board_config.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NOTIFICATION_FILTER_ALL = 0,
    NOTIFICATION_FILTER_CALLS,
    NOTIFICATION_FILTER_MESSAGES_SOCIAL,
    NOTIFICATION_FILTER_IMPORTANT_ONLY,
    NOTIFICATION_FILTER_COUNT,
} notification_filter_t;

typedef struct {
    char app_id[BOARD_ANCS_APP_ID_MAX_LEN];
    bool allowed;
    bool valid;
} notification_app_config_t;

esp_err_t storage_manager_init(void);
esp_err_t storage_manager_set_bonded(bool bonded);
esp_err_t storage_manager_get_bonded(bool *bonded);
esp_err_t storage_manager_set_display_inverted(bool inverted);
esp_err_t storage_manager_get_display_inverted(bool *inverted);
esp_err_t storage_manager_set_notification_filter(notification_filter_t filter);
esp_err_t storage_manager_get_notification_filter(notification_filter_t *filter);
esp_err_t storage_manager_track_notification_app(const char *app_id, bool allowed_if_new, bool *added);
bool storage_manager_is_notification_app_allowed(const char *app_id, bool *allowed);
esp_err_t storage_manager_set_notification_app_allowed(const char *app_id, bool allowed);
size_t storage_manager_get_notification_apps(notification_app_config_t *out_entries, size_t max_entries);
uint32_t storage_manager_get_notification_apps_revision(void);
esp_err_t storage_manager_set_calls_allowed(bool allowed);
esp_err_t storage_manager_get_calls_allowed(bool *allowed);

#ifdef __cplusplus
}
#endif
