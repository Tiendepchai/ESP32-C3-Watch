#include "storage_manager.h"

#include <stdint.h>
#include <string.h>
#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"

#define STORAGE_NAMESPACE    "app_cfg"
#define KEY_BONDED           "bonded"
#define KEY_DISP_INVERT      "disp_inv"
#define KEY_FILTER_LEGACY    "notif_fltr"
#define KEY_FILTER           "notif_fltr2"
#define KEY_APP_CATALOG      "app_catalog"

#define CATALOG_SCHEMA_VERSION 1U

typedef struct {
    uint32_t schema_version;
    uint32_t revision;
    bool calls_allowed;
    uint8_t reserved[3];
    notification_app_config_t entries[BOARD_NOTIFICATION_APP_CONFIG_MAX];
} notification_app_catalog_blob_t;

/* v0 layout: identical to v1 but without the schema_version prefix.
 * Kept ONLY to migrate existing NVS blobs written before versioning. */
typedef struct {
    uint32_t revision;
    bool calls_allowed;
    uint8_t reserved[3];
    notification_app_config_t entries[BOARD_NOTIFICATION_APP_CONFIG_MAX];
} notification_app_catalog_blob_v0_t;

static const char *TAG = BOARD_TAG_STORAGE;

static nvs_handle_t s_nvs_handle;
static SemaphoreHandle_t s_lock;
static bool s_initialized;
static bool s_catalog_full_warned;
static notification_app_catalog_blob_t s_app_catalog;

static esp_err_t storage_manager_read_u8(const char *key, uint8_t default_value, uint8_t *value)
{
    esp_err_t rc = nvs_get_u8(s_nvs_handle, key, value);
    if (rc == ESP_ERR_NVS_NOT_FOUND) {
        *value = default_value;
        return ESP_OK;
    }
    return rc;
}

static esp_err_t storage_manager_write_u8(const char *key, uint8_t value)
{
    esp_err_t rc = nvs_set_u8(s_nvs_handle, key, value);
    if (rc != ESP_OK) {
        return rc;
    }
    return nvs_commit(s_nvs_handle);
}

static int storage_manager_find_notification_app_index_locked(const char *app_id)
{
    if (app_id == NULL || app_id[0] == '\0') {
        return -1;
    }

    for (size_t i = 0; i < BOARD_NOTIFICATION_APP_CONFIG_MAX; ++i) {
        if (!s_app_catalog.entries[i].valid) {
            continue;
        }
        if (strcmp(s_app_catalog.entries[i].app_id, app_id) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static size_t storage_manager_find_notification_app_slot_locked(void)
{
    for (size_t i = 0; i < BOARD_NOTIFICATION_APP_CONFIG_MAX; ++i) {
        if (!s_app_catalog.entries[i].valid) {
            return i;
        }
    }

    return SIZE_MAX;
}

static esp_err_t storage_manager_write_catalog_locked(void)
{
    esp_err_t rc;

    rc = nvs_set_blob(s_nvs_handle, KEY_APP_CATALOG, &s_app_catalog, sizeof(s_app_catalog));
    if (rc != ESP_OK) {
        return rc;
    }

    return nvs_commit(s_nvs_handle);
}

static void storage_manager_init_catalog_defaults(void)
{
    memset(&s_app_catalog, 0, sizeof(s_app_catalog));
    s_app_catalog.schema_version = CATALOG_SCHEMA_VERSION;
    s_app_catalog.calls_allowed = true;
    s_catalog_full_warned = false;
}

static esp_err_t storage_manager_load_catalog_locked(void)
{
    size_t size = 0;
    esp_err_t rc;

    storage_manager_init_catalog_defaults();

    rc = nvs_get_blob(s_nvs_handle, KEY_APP_CATALOG, NULL, &size);
    if (rc == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (rc != ESP_OK) {
        return rc;
    }

    if (size == sizeof(notification_app_catalog_blob_t)) {
        notification_app_catalog_blob_t blob = {0};
        size_t read_size = sizeof(blob);
        rc = nvs_get_blob(s_nvs_handle, KEY_APP_CATALOG, &blob, &read_size);
        if (rc != ESP_OK) {
            return rc;
        }
        if (blob.schema_version != CATALOG_SCHEMA_VERSION) {
            ESP_LOGW(TAG, "catalog schema_version=%u unknown (expected %u), resetting",
                     (unsigned)blob.schema_version, (unsigned)CATALOG_SCHEMA_VERSION);
            storage_manager_init_catalog_defaults();
            return ESP_OK;
        }
        s_app_catalog = blob;
        return ESP_OK;
    }

    if (size == sizeof(notification_app_catalog_blob_v0_t)) {
        notification_app_catalog_blob_v0_t v0 = {0};
        size_t read_size = sizeof(v0);
        rc = nvs_get_blob(s_nvs_handle, KEY_APP_CATALOG, &v0, &read_size);
        if (rc != ESP_OK) {
            return rc;
        }
        ESP_LOGI(TAG, "migrating notification app catalog v0 -> v%u", (unsigned)CATALOG_SCHEMA_VERSION);
        s_app_catalog.schema_version = CATALOG_SCHEMA_VERSION;
        s_app_catalog.revision = v0.revision;
        s_app_catalog.calls_allowed = v0.calls_allowed;
        memcpy(s_app_catalog.reserved, v0.reserved, sizeof(s_app_catalog.reserved));
        memcpy(s_app_catalog.entries, v0.entries, sizeof(s_app_catalog.entries));
        return storage_manager_write_catalog_locked();
    }

    ESP_LOGW(TAG, "notification app catalog size mismatch=%u, resetting", (unsigned)size);
    storage_manager_init_catalog_defaults();
    return ESP_OK;
}

esp_err_t storage_manager_init(void)
{
    esp_err_t rc = nvs_flash_init();
    if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs re-init, erasing");
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs_flash_erase failed");
        rc = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(rc, TAG, "nvs_flash_init failed");

    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
        ESP_RETURN_ON_FALSE(s_lock != NULL, ESP_ERR_NO_MEM, TAG, "failed to create NVS lock");
    }

    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &s_nvs_handle), TAG, "nvs_open failed");
    xSemaphoreTake(s_lock, portMAX_DELAY);
    rc = storage_manager_load_catalog_locked();
    /* One-time migration: drop legacy filter key. Old enum had 5 values
     * (ALL/CALLS/MESSAGES_SOCIAL/IMPORTANT_ONLY/NAVIGATION); new enum has 3
     * (ALL/CALLS/NAVIGATION). Erase to avoid silent re-mapping of old values. */
    esp_err_t erase_rc = nvs_erase_key(s_nvs_handle, KEY_FILTER_LEGACY);
    if (erase_rc == ESP_OK) {
        ESP_LOGI(TAG, "erased legacy filter key '%s'", KEY_FILTER_LEGACY);
        nvs_commit(s_nvs_handle);
    } else if (erase_rc != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "failed to erase legacy filter key: %s", esp_err_to_name(erase_rc));
    }
    xSemaphoreGive(s_lock);
    ESP_RETURN_ON_ERROR(rc, TAG, "failed to load notification app catalog");
    s_initialized = true;
    ESP_LOGI(TAG, "storage initialized");
    return ESP_OK;
}

esp_err_t storage_manager_set_bonded(bool bonded)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "storage not initialized");
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t rc = storage_manager_write_u8(KEY_BONDED, bonded ? 1U : 0U);
    xSemaphoreGive(s_lock);
    ESP_RETURN_ON_ERROR(rc, TAG, "failed to write bonded flag");
    return ESP_OK;
}

esp_err_t storage_manager_get_bonded(bool *bonded)
{
    uint8_t value = 0;
    ESP_RETURN_ON_FALSE(bonded != NULL, ESP_ERR_INVALID_ARG, TAG, "bonded pointer is null");
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "storage not initialized");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t rc = storage_manager_read_u8(KEY_BONDED, 0U, &value);
    xSemaphoreGive(s_lock);
    ESP_RETURN_ON_ERROR(rc, TAG, "failed to read bonded flag");

    *bonded = (value != 0U);
    return ESP_OK;
}

esp_err_t storage_manager_set_display_inverted(bool inverted)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "storage not initialized");
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t rc = storage_manager_write_u8(KEY_DISP_INVERT, inverted ? 1U : 0U);
    xSemaphoreGive(s_lock);
    ESP_RETURN_ON_ERROR(rc, TAG, "failed to write display inversion flag");
    return ESP_OK;
}

esp_err_t storage_manager_get_display_inverted(bool *inverted)
{
    uint8_t value = 0;
    ESP_RETURN_ON_FALSE(inverted != NULL, ESP_ERR_INVALID_ARG, TAG, "inverted pointer is null");
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "storage not initialized");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t rc = storage_manager_read_u8(KEY_DISP_INVERT, 0U, &value);
    xSemaphoreGive(s_lock);
    ESP_RETURN_ON_ERROR(rc, TAG, "failed to read display inversion flag");

    *inverted = (value != 0U);
    return ESP_OK;
}

esp_err_t storage_manager_set_notification_filter(notification_filter_t filter)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "storage not initialized");
    ESP_RETURN_ON_FALSE(filter < NOTIFICATION_FILTER_COUNT, ESP_ERR_INVALID_ARG, TAG, "invalid filter=%d", filter);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t rc = storage_manager_write_u8(KEY_FILTER, (uint8_t)filter);
    xSemaphoreGive(s_lock);
    ESP_RETURN_ON_ERROR(rc, TAG, "failed to write notification filter");
    return ESP_OK;
}

esp_err_t storage_manager_get_notification_filter(notification_filter_t *filter)
{
    uint8_t value = NOTIFICATION_FILTER_ALL;

    ESP_RETURN_ON_FALSE(filter != NULL, ESP_ERR_INVALID_ARG, TAG, "filter pointer is null");
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "storage not initialized");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t rc = storage_manager_read_u8(KEY_FILTER, NOTIFICATION_FILTER_ALL, &value);
    xSemaphoreGive(s_lock);
    ESP_RETURN_ON_ERROR(rc, TAG, "failed to read notification filter");

    if (value >= NOTIFICATION_FILTER_COUNT) {
        value = NOTIFICATION_FILTER_ALL;
    }

    *filter = (notification_filter_t)value;
    return ESP_OK;
}

esp_err_t storage_manager_track_notification_app(const char *app_id, bool allowed_if_new, bool *added)
{
    int index;
    size_t slot;
    esp_err_t rc = ESP_OK;
    bool inserted = false;

    ESP_RETURN_ON_FALSE(app_id != NULL && app_id[0] != '\0', ESP_ERR_INVALID_ARG, TAG, "app_id is empty");
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "storage not initialized");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    index = storage_manager_find_notification_app_index_locked(app_id);
    if (index < 0) {
        slot = storage_manager_find_notification_app_slot_locked();
        if (slot == SIZE_MAX) {
            if (!s_catalog_full_warned) {
                ESP_LOGW(TAG, "notification app catalog full, leaving new apps untracked");
                s_catalog_full_warned = true;
            }
        } else {
            memset(&s_app_catalog.entries[slot], 0, sizeof(s_app_catalog.entries[slot]));
            strlcpy(s_app_catalog.entries[slot].app_id, app_id, sizeof(s_app_catalog.entries[slot].app_id));
            s_app_catalog.entries[slot].allowed = allowed_if_new;
            s_app_catalog.entries[slot].valid = true;
            s_app_catalog.revision++;
            rc = storage_manager_write_catalog_locked();
            inserted = (rc == ESP_OK);
            if (rc == ESP_OK) {
                ESP_LOGI(TAG, "tracked notification app %s allowed=%d", app_id, allowed_if_new);
            }
        }
    }
    xSemaphoreGive(s_lock);

    if (added != NULL) {
        *added = inserted;
    }
    ESP_RETURN_ON_ERROR(rc, TAG, "failed to persist notification app");
    return ESP_OK;
}

bool storage_manager_is_notification_app_allowed(const char *app_id, bool *allowed)
{
    int index;
    bool found;

    if (app_id == NULL || app_id[0] == '\0' || allowed == NULL || !s_initialized) {
        return false;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    index = storage_manager_find_notification_app_index_locked(app_id);
    found = index >= 0;
    if (found) {
        *allowed = s_app_catalog.entries[index].allowed;
    }
    xSemaphoreGive(s_lock);
    return found;
}

esp_err_t storage_manager_set_notification_app_allowed(const char *app_id, bool allowed)
{
    int index;
    size_t slot;
    esp_err_t rc = ESP_OK;

    ESP_RETURN_ON_FALSE(app_id != NULL && app_id[0] != '\0', ESP_ERR_INVALID_ARG, TAG, "app_id is empty");
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "storage not initialized");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    index = storage_manager_find_notification_app_index_locked(app_id);
    if (index < 0) {
        slot = storage_manager_find_notification_app_slot_locked();
        if (slot == SIZE_MAX) {
            rc = ESP_ERR_NO_MEM;
            ESP_LOGW(TAG, "notification app catalog full, refusing to insert %s", app_id);
        } else {
            memset(&s_app_catalog.entries[slot], 0, sizeof(s_app_catalog.entries[slot]));
            strlcpy(s_app_catalog.entries[slot].app_id, app_id, sizeof(s_app_catalog.entries[slot].app_id));
            s_app_catalog.entries[slot].valid = true;
            s_app_catalog.entries[slot].allowed = allowed;
            s_app_catalog.revision++;
            rc = storage_manager_write_catalog_locked();
        }
    } else if (s_app_catalog.entries[index].allowed != allowed) {
        s_app_catalog.entries[index].allowed = allowed;
        s_app_catalog.revision++;
        rc = storage_manager_write_catalog_locked();
    }
    xSemaphoreGive(s_lock);

    ESP_RETURN_ON_ERROR(rc, TAG, "failed to persist notification app state");
    return ESP_OK;
}

size_t storage_manager_get_notification_apps(notification_app_config_t *out_entries, size_t max_entries)
{
    size_t count = 0;

    if (!s_initialized) {
        return 0;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < BOARD_NOTIFICATION_APP_CONFIG_MAX; ++i) {
        if (!s_app_catalog.entries[i].valid) {
            continue;
        }
        if (out_entries != NULL && count < max_entries) {
            out_entries[count] = s_app_catalog.entries[i];
        }
        count++;
    }
    xSemaphoreGive(s_lock);

    if (out_entries == NULL || max_entries == 0U) {
        return count;
    }

    return count < max_entries ? count : max_entries;
}

uint32_t storage_manager_get_notification_apps_revision(void)
{
    uint32_t revision = 0;

    if (!s_initialized) {
        return 0;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    revision = s_app_catalog.revision;
    xSemaphoreGive(s_lock);
    return revision;
}

esp_err_t storage_manager_set_calls_allowed(bool allowed)
{
    esp_err_t rc = ESP_OK;

    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "storage not initialized");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_app_catalog.calls_allowed != allowed) {
        s_app_catalog.calls_allowed = allowed;
        s_app_catalog.revision++;
        rc = storage_manager_write_catalog_locked();
    }
    xSemaphoreGive(s_lock);

    ESP_RETURN_ON_ERROR(rc, TAG, "failed to persist calls_allowed");
    return ESP_OK;
}

esp_err_t storage_manager_get_calls_allowed(bool *allowed)
{
    ESP_RETURN_ON_FALSE(allowed != NULL, ESP_ERR_INVALID_ARG, TAG, "allowed pointer is null");
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "storage not initialized");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    *allowed = s_app_catalog.calls_allowed;
    xSemaphoreGive(s_lock);
    return ESP_OK;
}
