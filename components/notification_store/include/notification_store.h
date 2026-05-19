#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "board_config.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct notification_store notification_store_t;
typedef notification_store_t *notification_store_handle_t;

typedef struct {
    uint32_t uid;
    uint8_t event_id;
    uint8_t event_flags;
    uint8_t category_id;
    uint8_t category_count;
    char app_id[BOARD_ANCS_APP_ID_MAX_LEN];
    char title[BOARD_ANCS_TITLE_MAX_LEN];
    char message[BOARD_ANCS_MESSAGE_MAX_LEN];
    uint64_t timestamp_ms;
    bool details_complete;
    bool valid;
} notification_record_t;

esp_err_t notification_store_init(notification_store_handle_t *out_handle);
void notification_store_deinit(notification_store_handle_t handle);
esp_err_t notification_store_push(notification_store_handle_t handle, const notification_record_t *record);
esp_err_t notification_store_upsert(notification_store_handle_t handle, const notification_record_t *record);
bool notification_store_get_latest(notification_store_handle_t handle, notification_record_t *out_record);
size_t notification_store_get_all(notification_store_handle_t handle, notification_record_t *out_records, size_t max_records);
size_t notification_store_snapshot_uids(notification_store_handle_t handle, uint32_t *out_uids, size_t max_uids);
bool notification_store_find_by_uid(notification_store_handle_t handle, uint32_t uid, notification_record_t *out_record);
bool notification_store_remove_by_uid(notification_store_handle_t handle, uint32_t uid);
void notification_store_clear(notification_store_handle_t handle);
size_t notification_store_count(notification_store_handle_t handle);

#ifdef __cplusplus
}
#endif
