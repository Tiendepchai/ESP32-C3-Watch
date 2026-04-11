#include "notification_store.h"

#include <stdlib.h>
#include <string.h>
#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct notification_store {
    notification_record_t records[BOARD_NOTIFICATION_QUEUE_MAX];
    size_t newest_index;
    size_t count;
    SemaphoreHandle_t lock;
};

static const char *TAG = BOARD_TAG_STORE;

static int notification_store_find_index_locked(notification_store_handle_t handle, uint32_t uid)
{
    for (size_t i = 0; i < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        if (handle->records[i].valid && handle->records[i].uid == uid) {
            return (int)i;
        }
    }
    return -1;
}

static size_t notification_store_find_oldest_or_free_locked(notification_store_handle_t handle)
{
    uint64_t oldest_ts = UINT64_MAX;
    size_t oldest_index = 0;

    for (size_t i = 0; i < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        if (!handle->records[i].valid) {
            return i;
        }
        if (handle->records[i].timestamp_ms <= oldest_ts) {
            oldest_ts = handle->records[i].timestamp_ms;
            oldest_index = i;
        }
    }
    return oldest_index;
}

esp_err_t notification_store_init(notification_store_handle_t *out_handle)
{
    notification_store_handle_t handle = NULL;

    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");
    handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory for notification store");

    handle->lock = xSemaphoreCreateMutex();
    if (handle->lock == NULL) {
        free(handle);
        return ESP_ERR_NO_MEM;
    }

    *out_handle = handle;
    ESP_LOGI(TAG, "notification store initialized, capacity=%d", BOARD_NOTIFICATION_QUEUE_MAX);
    return ESP_OK;
}

void notification_store_deinit(notification_store_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    if (handle->lock != NULL) {
        vSemaphoreDelete(handle->lock);
    }
    free(handle);
}

esp_err_t notification_store_push(notification_store_handle_t handle, const notification_record_t *record)
{
    return notification_store_upsert(handle, record);
}

esp_err_t notification_store_upsert(notification_store_handle_t handle, const notification_record_t *record)
{
    int existing_index;
    size_t target_index;

    ESP_RETURN_ON_FALSE(handle != NULL && record != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    existing_index = notification_store_find_index_locked(handle, record->uid);
    target_index = (existing_index >= 0) ? (size_t)existing_index : notification_store_find_oldest_or_free_locked(handle);

    handle->records[target_index] = *record;
    handle->records[target_index].valid = true;
    handle->newest_index = target_index;
    if (existing_index < 0 && handle->count < BOARD_NOTIFICATION_QUEUE_MAX) {
        handle->count++;
    }
    xSemaphoreGive(handle->lock);
    return ESP_OK;
}

bool notification_store_get_latest(notification_store_handle_t handle, notification_record_t *out_record)
{
    bool found = false;
    uint64_t newest_ts = 0;

    if (handle == NULL || out_record == NULL) {
        return false;
    }

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    for (size_t i = 0; i < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        if (handle->records[i].valid && (!found || handle->records[i].timestamp_ms >= newest_ts)) {
            newest_ts = handle->records[i].timestamp_ms;
            *out_record = handle->records[i];
            found = true;
        }
    }
    xSemaphoreGive(handle->lock);
    return found;
}

size_t notification_store_get_all(notification_store_handle_t handle, notification_record_t *out_records, size_t max_records)
{
    size_t count = 0;

    if (handle == NULL || out_records == NULL || max_records == 0U) {
        return 0;
    }

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    for (size_t i = 0; i < BOARD_NOTIFICATION_QUEUE_MAX && count < max_records; ++i) {
        if (handle->records[i].valid) {
            out_records[count++] = handle->records[i];
        }
    }
    xSemaphoreGive(handle->lock);

    for (size_t i = 1; i < count; ++i) {
        notification_record_t current = out_records[i];
        size_t j = i;

        while (j > 0 && out_records[j - 1U].timestamp_ms < current.timestamp_ms) {
            out_records[j] = out_records[j - 1U];
            j--;
        }
        out_records[j] = current;
    }

    return count;
}

bool notification_store_find_by_uid(notification_store_handle_t handle, uint32_t uid, notification_record_t *out_record)
{
    int index;

    if (handle == NULL || out_record == NULL) {
        return false;
    }

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    index = notification_store_find_index_locked(handle, uid);
    if (index >= 0) {
        *out_record = handle->records[index];
    }
    xSemaphoreGive(handle->lock);
    return index >= 0;
}

bool notification_store_remove_by_uid(notification_store_handle_t handle, uint32_t uid)
{
    int index;
    bool removed = false;

    if (handle == NULL) {
        return false;
    }

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    index = notification_store_find_index_locked(handle, uid);
    if (index >= 0) {
        memset(&handle->records[index], 0, sizeof(handle->records[index]));
        if (handle->count > 0) {
            handle->count--;
        }
        removed = true;
    }
    xSemaphoreGive(handle->lock);
    return removed;
}

void notification_store_clear(notification_store_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    memset(handle->records, 0, sizeof(handle->records));
    handle->count = 0;
    handle->newest_index = 0;
    xSemaphoreGive(handle->lock);
    ESP_LOGI(TAG, "notification store cleared");
}

size_t notification_store_count(notification_store_handle_t handle)
{
    size_t count = 0;

    if (handle == NULL) {
        return 0;
    }

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    count = handle->count;
    xSemaphoreGive(handle->lock);
    return count;
}
