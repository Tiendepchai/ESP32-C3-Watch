#include "notification_store.h"

#include <stdlib.h>
#include <string.h>
#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    uint32_t uid;
    uint8_t event_id;
    uint8_t event_flags;
    uint8_t category_id;
    uint8_t category_count;
    uint64_t timestamp_ms;
    bool details_complete;
    bool valid;
    char *app_id;
    char *title;
    char *message;
} notification_store_entry_t;

struct notification_store {
    notification_store_entry_t entries[BOARD_NOTIFICATION_QUEUE_MAX];
    size_t newest_index;
    size_t count;
    SemaphoreHandle_t lock;
};

static const char *TAG = BOARD_TAG_STORE;

static char *notification_store_strdup_safe(const char *s)
{
    if (s == NULL || s[0] == '\0') {
        return NULL;
    }
    size_t len = strlen(s);
    char *p = malloc(len + 1U);
    if (p == NULL) {
        ESP_LOGW(TAG, "strdup failed for %u bytes", (unsigned)(len + 1U));
        return NULL;
    }
    memcpy(p, s, len + 1U);
    return p;
}

static void notification_store_entry_free_strings(notification_store_entry_t *entry)
{
    free(entry->app_id);
    free(entry->title);
    free(entry->message);
    entry->app_id = NULL;
    entry->title = NULL;
    entry->message = NULL;
}

static void notification_store_entry_reset(notification_store_entry_t *entry)
{
    notification_store_entry_free_strings(entry);
    entry->uid = 0;
    entry->event_id = 0;
    entry->event_flags = 0;
    entry->category_id = 0;
    entry->category_count = 0;
    entry->timestamp_ms = 0;
    entry->details_complete = false;
    entry->valid = false;
}

static void notification_store_entry_assign(notification_store_entry_t *entry, const notification_record_t *record)
{
    notification_store_entry_free_strings(entry);
    entry->uid = record->uid;
    entry->event_id = record->event_id;
    entry->event_flags = record->event_flags;
    entry->category_id = record->category_id;
    entry->category_count = record->category_count;
    entry->timestamp_ms = record->timestamp_ms;
    entry->details_complete = record->details_complete;
    entry->valid = true;
    entry->app_id = notification_store_strdup_safe(record->app_id);
    entry->title = notification_store_strdup_safe(record->title);
    entry->message = notification_store_strdup_safe(record->message);
}

static void notification_store_entry_to_record(notification_record_t *out, const notification_store_entry_t *entry)
{
    memset(out, 0, sizeof(*out));
    out->uid = entry->uid;
    out->event_id = entry->event_id;
    out->event_flags = entry->event_flags;
    out->category_id = entry->category_id;
    out->category_count = entry->category_count;
    out->timestamp_ms = entry->timestamp_ms;
    out->details_complete = entry->details_complete;
    out->valid = entry->valid;
    if (entry->app_id != NULL) {
        strlcpy(out->app_id, entry->app_id, sizeof(out->app_id));
    }
    if (entry->title != NULL) {
        strlcpy(out->title, entry->title, sizeof(out->title));
    }
    if (entry->message != NULL) {
        strlcpy(out->message, entry->message, sizeof(out->message));
    }
}

static int notification_store_find_index_locked(notification_store_handle_t handle, uint32_t uid)
{
    for (size_t i = 0; i < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        if (handle->entries[i].valid && handle->entries[i].uid == uid) {
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
        if (!handle->entries[i].valid) {
            return i;
        }
        if (handle->entries[i].timestamp_ms <= oldest_ts) {
            oldest_ts = handle->entries[i].timestamp_ms;
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
    for (size_t i = 0; i < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        notification_store_entry_free_strings(&handle->entries[i]);
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
    ESP_RETURN_ON_FALSE(record->uid != 0U, ESP_ERR_INVALID_ARG, TAG, "refusing to store notification with uid=0");

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    existing_index = notification_store_find_index_locked(handle, record->uid);
    target_index = (existing_index >= 0) ? (size_t)existing_index : notification_store_find_oldest_or_free_locked(handle);

    notification_store_entry_assign(&handle->entries[target_index], record);
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
    size_t newest_idx = 0;

    if (handle == NULL || out_record == NULL) {
        return false;
    }

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    for (size_t i = 0; i < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        if (handle->entries[i].valid && (!found || handle->entries[i].timestamp_ms >= newest_ts)) {
            newest_ts = handle->entries[i].timestamp_ms;
            newest_idx = i;
            found = true;
        }
    }
    if (found) {
        notification_store_entry_to_record(out_record, &handle->entries[newest_idx]);
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
        if (handle->entries[i].valid) {
            notification_store_entry_to_record(&out_records[count++], &handle->entries[i]);
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

size_t notification_store_snapshot_uids(notification_store_handle_t handle, uint32_t *out_uids, size_t max_uids)
{
    typedef struct { uint32_t uid; uint64_t ts; } uid_ts_t;
    uid_ts_t pairs[BOARD_NOTIFICATION_QUEUE_MAX];
    size_t count = 0;

    if (handle == NULL || out_uids == NULL || max_uids == 0U) {
        return 0;
    }

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    for (size_t i = 0; i < BOARD_NOTIFICATION_QUEUE_MAX && count < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        if (handle->entries[i].valid) {
            pairs[count].uid = handle->entries[i].uid;
            pairs[count].ts = handle->entries[i].timestamp_ms;
            count++;
        }
    }
    xSemaphoreGive(handle->lock);

    for (size_t i = 1; i < count; ++i) {
        uid_ts_t current = pairs[i];
        size_t j = i;
        while (j > 0 && pairs[j - 1U].ts < current.ts) {
            pairs[j] = pairs[j - 1U];
            j--;
        }
        pairs[j] = current;
    }

    if (count > max_uids) {
        count = max_uids;
    }
    for (size_t i = 0; i < count; ++i) {
        out_uids[i] = pairs[i].uid;
    }
    return count;
}

bool notification_store_find_by_uid(notification_store_handle_t handle, uint32_t uid, notification_record_t *out_record)
{
    int index;
    bool found = false;

    if (handle == NULL || out_record == NULL) {
        return false;
    }

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    index = notification_store_find_index_locked(handle, uid);
    if (index >= 0) {
        notification_store_entry_to_record(out_record, &handle->entries[index]);
        found = true;
    }
    xSemaphoreGive(handle->lock);
    return found;
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
        notification_store_entry_reset(&handle->entries[index]);
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
    for (size_t i = 0; i < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        notification_store_entry_reset(&handle->entries[i]);
    }
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
