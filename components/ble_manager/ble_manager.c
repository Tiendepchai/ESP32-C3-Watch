#include "ble_manager.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "ancs_client.h"
#include "board_config.h"
#include "cts_client.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "os/os_mbuf.h"
#include "nimble/hci_common.h"
#include "host/util/util.h"
#include "host/ble_store.h"
#include "host/ble_sm.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "storage_manager.h"

static const char *TAG = BOARD_TAG_BLE;
static const char *BLE_MANAGER_CALLS_PSEUDO_APP_ID = "__calls__";

typedef enum {
    BLE_MANAGER_CFG_CHR_SUMMARY = 0,
    BLE_MANAGER_CFG_CHR_PAGE,
    BLE_MANAGER_CFG_CHR_CATALOG,
    BLE_MANAGER_CFG_CHR_TOGGLE,
    BLE_MANAGER_CFG_CHR_NAVIGATION,
} ble_manager_cfg_chr_t;

typedef struct {
    ble_manager_config_t cfg;
    esp_timer_handle_t reconnect_timer;
    uint8_t own_addr_type;
    uint8_t config_page;
    uint16_t conn_handle;
    uint16_t cfg_summary_handle;
    uint16_t cfg_page_handle;
    uint16_t cfg_catalog_handle;
    uint16_t cfg_navigation_handle;
    bool synced;
    bool advertising;
    bool had_bond_before_connect;
    bool connected;
    bool secured;
    bool directed_adv_attempted;
    bool bond_recovery_done;
    bool bond_just_established;
    ble_manager_navigation_state_t navigation;
} ble_manager_ctx_t;

static ble_manager_ctx_t s_ble;

static const ble_uuid128_t s_ancs_service_uuid = BLE_UUID128_INIT(
    0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4,
    0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79);

static const ble_uuid128_t s_config_service_uuid = BLE_UUID128_INIT(
    0x60, 0x5E, 0x4D, 0x3C, 0x2B, 0x9A, 0x01, 0x9C,
    0x5B, 0x4A, 0x3E, 0x4F, 0x00, 0x10, 0x4A, 0x9F);
static const ble_uuid128_t s_config_summary_uuid = BLE_UUID128_INIT(
    0x60, 0x5E, 0x4D, 0x3C, 0x2B, 0x9A, 0x01, 0x9C,
    0x5B, 0x4A, 0x3E, 0x4F, 0x01, 0x10, 0x4A, 0x9F);
static const ble_uuid128_t s_config_page_uuid = BLE_UUID128_INIT(
    0x60, 0x5E, 0x4D, 0x3C, 0x2B, 0x9A, 0x01, 0x9C,
    0x5B, 0x4A, 0x3E, 0x4F, 0x02, 0x10, 0x4A, 0x9F);
static const ble_uuid128_t s_config_catalog_uuid = BLE_UUID128_INIT(
    0x60, 0x5E, 0x4D, 0x3C, 0x2B, 0x9A, 0x01, 0x9C,
    0x5B, 0x4A, 0x3E, 0x4F, 0x03, 0x10, 0x4A, 0x9F);
static const ble_uuid128_t s_config_toggle_uuid = BLE_UUID128_INIT(
    0x60, 0x5E, 0x4D, 0x3C, 0x2B, 0x9A, 0x01, 0x9C,
    0x5B, 0x4A, 0x3E, 0x4F, 0x04, 0x10, 0x4A, 0x9F);
static const ble_uuid128_t s_config_navigation_uuid = BLE_UUID128_INIT(
    0x60, 0x5E, 0x4D, 0x3C, 0x2B, 0x9A, 0x01, 0x9C,
    0x5B, 0x4A, 0x3E, 0x4F, 0x05, 0x10, 0x4A, 0x9F);

void ble_store_config_init(void);
static int ble_manager_gap_event(struct ble_gap_event *event, void *arg);
static int ble_manager_config_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt, void *arg);
static esp_err_t ble_manager_init_config_service(void);
static void ble_manager_update_config_characteristics(void);
static int ble_manager_get_bonded_peer_count(void);

static const struct ble_gatt_svc_def s_config_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_config_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &s_config_summary_uuid.u,
                .access_cb = ble_manager_config_access_cb,
                .arg = (void *)(intptr_t)BLE_MANAGER_CFG_CHR_SUMMARY,
                .val_handle = &s_ble.cfg_summary_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                .uuid = &s_config_page_uuid.u,
                .access_cb = ble_manager_config_access_cb,
                .arg = (void *)(intptr_t)BLE_MANAGER_CFG_CHR_PAGE,
                .val_handle = &s_ble.cfg_page_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC |
                         BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC |
                         BLE_GATT_CHR_F_NOTIFY,
            },
            {
                .uuid = &s_config_catalog_uuid.u,
                .access_cb = ble_manager_config_access_cb,
                .arg = (void *)(intptr_t)BLE_MANAGER_CFG_CHR_CATALOG,
                .val_handle = &s_ble.cfg_catalog_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                .uuid = &s_config_toggle_uuid.u,
                .access_cb = ble_manager_config_access_cb,
                .arg = (void *)(intptr_t)BLE_MANAGER_CFG_CHR_TOGGLE,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
            },
            {
                .uuid = &s_config_navigation_uuid.u,
                .access_cb = ble_manager_config_access_cb,
                .arg = (void *)(intptr_t)BLE_MANAGER_CFG_CHR_NAVIGATION,
                .val_handle = &s_ble.cfg_navigation_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC |
                         BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC |
                         BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 },
        },
    },
    { 0 },
};

static void ble_manager_emit_state(ble_manager_state_t state)
{
    ESP_LOGI(TAG, "state -> %s", ble_manager_state_to_string(state));
    if (s_ble.cfg.state_cb != NULL) {
        s_ble.cfg.state_cb(state, s_ble.cfg.user_ctx);
    }
}

static void ble_manager_emit_bond(bool bonded)
{
    if (s_ble.cfg.bond_cb != NULL) {
        s_ble.cfg.bond_cb(bonded, s_ble.cfg.user_ctx);
    }
}

static void ble_manager_emit_config_changed(void)
{
    if (s_ble.cfg.config_changed_cb != NULL) {
        s_ble.cfg.config_changed_cb(s_ble.cfg.user_ctx);
    }
}

static void ble_manager_emit_navigation(const ble_manager_navigation_state_t *state)
{
    if (s_ble.cfg.navigation_cb != NULL && state != NULL) {
        s_ble.cfg.navigation_cb(state, s_ble.cfg.user_ctx);
    }
}

static void ble_manager_handle_secure_link_ready(uint16_t conn_handle, bool bonded, int key_size)
{
    int bonded_peers = ble_manager_get_bonded_peer_count();

    if (!s_ble.connected || conn_handle != s_ble.conn_handle) {
        ESP_LOGW(TAG, "ignoring stale secure-link event for conn_handle=%u", conn_handle);
        return;
    }

    ble_manager_emit_bond(bonded);
    ESP_LOGI(TAG, "secure link ready, bonded=%d key_size=%d", bonded, key_size);
    ESP_LOGI(TAG, "bond store peers after secure=%d (had_bond_before_connect=%d)",
             bonded_peers, s_ble.had_bond_before_connect);

    s_ble.secured = true;
    s_ble.bond_recovery_done = false;
    s_ble.bond_just_established = (!s_ble.had_bond_before_connect && bonded);
    ble_manager_emit_state(BLE_MANAGER_STATE_SECURED);

    if (s_ble.bond_just_established) {
        ESP_LOGI(TAG, "new bond established; waiting for bonded reconnect before ANCS discovery");
        ESP_LOGI(TAG, "if peer stays connected, fallback ANCS discovery starts in %u ms",
                 BOARD_ANCS_DISCOVERY_FIRST_BOND_MS);
        if (cts_client_start_discovery(conn_handle) != ESP_OK) {
            ESP_LOGW(TAG, "failed to start first-bond CTS discovery");
        }
        if (ancs_client_schedule_discovery(conn_handle, BOARD_ANCS_DISCOVERY_FIRST_BOND_MS) != ESP_OK) {
            ESP_LOGW(TAG, "failed to schedule first-bond fallback ANCS discovery");
        }
        return;
    }

    if (ancs_client_schedule_discovery(conn_handle, BOARD_ANCS_DISCOVERY_DELAY_MS) != ESP_OK) {
        ESP_LOGW(TAG, "failed to schedule ANCS discovery");
    }
    if (cts_client_start_discovery(conn_handle) != ESP_OK) {
        ESP_LOGW(TAG, "failed to start CTS discovery");
    }
}

static void ble_manager_ancs_ready_cb(bool attr_channel_ready, void *user_ctx)
{
    (void)user_ctx;

    if (!s_ble.connected || !s_ble.secured || s_ble.conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGW(TAG, "ignoring stale ANCS ready callback while link is not active");
        return;
    }

    ESP_LOGI(TAG, "ANCS ready, attr channel %s", attr_channel_ready ? "available" : "not available");
    ble_manager_emit_state(BLE_MANAGER_STATE_ANCS_READY);
}

static void ble_manager_ancs_notification_cb(const notification_record_t *record, bool complete, void *user_ctx)
{
    (void)user_ctx;

    if (!s_ble.connected || !s_ble.secured || s_ble.conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGD(TAG, "dropping stale ANCS notification callback while link is not active");
        return;
    }
    if (record == NULL || record->uid == 0U) {
        ESP_LOGD(TAG, "dropping malformed ANCS notification callback");
        return;
    }

    if (s_ble.cfg.notification_cb != NULL && record != NULL) {
        s_ble.cfg.notification_cb(record, complete, s_ble.cfg.user_ctx);
    }
}

static char *ble_manager_addr_to_str(const ble_addr_t *addr, char *buf, size_t buf_len)
{
    if (addr == NULL || buf == NULL || buf_len < 18U) {
        return NULL;
    }
    snprintf(buf, buf_len, "%02X:%02X:%02X:%02X:%02X:%02X",
             addr->val[5], addr->val[4], addr->val[3], addr->val[2], addr->val[1], addr->val[0]);
    return buf;
}

bool ble_manager_has_bonded_peer(void)
{
    ble_addr_t peers[BOARD_NOTIFICATION_QUEUE_MAX] = {0};
    int count = BOARD_NOTIFICATION_QUEUE_MAX;
    int rc = ble_store_util_bonded_peers(peers, &count, BOARD_NOTIFICATION_QUEUE_MAX);
    return (rc == 0 && count > 0);
}

esp_err_t ble_manager_clear_bonds(void)
{
    int rc = ble_store_clear();
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_store_clear failed, rc=%d", rc);
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "all BLE bonds cleared");
    return ESP_OK;
}

esp_err_t ble_manager_request_notification_details(const notification_record_t *record, bool prioritize)
{
    return ancs_client_request_notification_details(record, prioritize);
}

static size_t ble_manager_get_config_entry_count(notification_app_config_t *entries, size_t max_entries, bool *calls_allowed)
{
    if (calls_allowed != NULL && storage_manager_get_calls_allowed(calls_allowed) != ESP_OK) {
        *calls_allowed = true;
    }

    return storage_manager_get_notification_apps(entries, max_entries);
}

static size_t ble_manager_get_config_total_entries(void)
{
    notification_app_config_t entries[BOARD_NOTIFICATION_APP_CONFIG_MAX];

    return 1U + ble_manager_get_config_entry_count(entries,
                                                   sizeof(entries) / sizeof(entries[0]),
                                                   NULL);
}

static uint8_t ble_manager_get_config_total_pages(void)
{
    size_t total_entries = ble_manager_get_config_total_entries();
    size_t pages = (total_entries + BOARD_NOTIFICATION_APP_PAGE_SIZE - 1U) / BOARD_NOTIFICATION_APP_PAGE_SIZE;

    return (uint8_t)(pages == 0U ? 1U : pages);
}

static void ble_manager_clamp_config_page(void)
{
    uint8_t total_pages = ble_manager_get_config_total_pages();

    if (s_ble.config_page >= total_pages) {
        s_ble.config_page = (uint8_t)(total_pages - 1U);
    }
}

static int ble_manager_read_flat_text(struct os_mbuf *om, char *buffer, size_t buffer_len)
{
    int payload_len;

    if (om == NULL || buffer == NULL || buffer_len == 0U) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    payload_len = OS_MBUF_PKTLEN(om);
    if (payload_len <= 0 || (size_t)payload_len >= buffer_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (os_mbuf_copydata(om, 0, payload_len, buffer) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    buffer[payload_len] = '\0';
    while (payload_len > 0 && (buffer[payload_len - 1] == '\n' || buffer[payload_len - 1] == '\r' || buffer[payload_len - 1] == ' ')) {
        buffer[payload_len - 1] = '\0';
        payload_len--;
    }
    return 0;
}

static int ble_manager_append_text(struct os_mbuf *om, const char *text)
{
    int rc;

    if (om == NULL || text == NULL) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    rc = os_mbuf_append(om, text, strlen(text));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static void ble_manager_reset_navigation_state(ble_manager_navigation_state_t *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
}

static void ble_manager_notify_navigation_changed(void)
{
    if (s_ble.cfg_navigation_handle != 0U) {
        ble_gatts_chr_updated(s_ble.cfg_navigation_handle);
    }
}

static void ble_manager_apply_navigation_state(const ble_manager_navigation_state_t *state)
{
    if (state == NULL) {
        return;
    }

    s_ble.navigation = *state;
    ble_manager_notify_navigation_changed();
    ble_manager_emit_navigation(&s_ble.navigation);
}

static void ble_manager_clear_navigation_state(void)
{
    ble_manager_navigation_state_t cleared = {0};

    ble_manager_reset_navigation_state(&cleared);
    ble_manager_apply_navigation_state(&cleared);
}

static int ble_manager_append_config_summary(struct os_mbuf *om)
{
    char summary[96];
    bool calls_allowed = true;
    size_t app_count = 0;

    app_count = storage_manager_get_notification_apps(NULL, 0);
    if (storage_manager_get_calls_allowed(&calls_allowed) != ESP_OK) {
        calls_allowed = true;
    }
    ble_manager_clamp_config_page();
    snprintf(summary, sizeof(summary),
             "version=1\npage=%u\npages=%u\ncount=%u\ncalls=%u\nrevision=%" PRIu32 "\n",
             s_ble.config_page,
             ble_manager_get_config_total_pages(),
             (unsigned)(app_count + 1U),
             calls_allowed ? 1U : 0U,
             storage_manager_get_notification_apps_revision());
    return ble_manager_append_text(om, summary);
}

static int ble_manager_append_config_catalog(struct os_mbuf *om)
{
    notification_app_config_t entries[BOARD_NOTIFICATION_APP_CONFIG_MAX] = {0};
    char catalog[(BOARD_NOTIFICATION_APP_PAGE_SIZE * (BOARD_ANCS_APP_ID_MAX_LEN + 8U)) + 32U];
    bool calls_allowed = true;
    size_t app_count;
    size_t start_index;
    size_t end_index;
    size_t offset = 0U;
    bool first_line = true;

    app_count = ble_manager_get_config_entry_count(entries, sizeof(entries) / sizeof(entries[0]), &calls_allowed);
    ble_manager_clamp_config_page();
    start_index = (size_t)s_ble.config_page * BOARD_NOTIFICATION_APP_PAGE_SIZE;
    end_index = start_index + BOARD_NOTIFICATION_APP_PAGE_SIZE;
    memset(catalog, 0, sizeof(catalog));

    for (size_t logical_index = start_index; logical_index < end_index; ++logical_index) {
        const char *app_id = NULL;
        bool allowed = false;
        int written;

        if (logical_index == 0U) {
            app_id = BLE_MANAGER_CALLS_PSEUDO_APP_ID;
            allowed = calls_allowed;
        } else if ((logical_index - 1U) < app_count) {
            app_id = entries[logical_index - 1U].app_id;
            allowed = entries[logical_index - 1U].allowed;
        } else {
            break;
        }

        written = snprintf(&catalog[offset], sizeof(catalog) - offset,
                           "%s%s|%u",
                           first_line ? "" : "\n",
                           app_id,
                           allowed ? 1U : 0U);
        if (written < 0 || (size_t)written >= (sizeof(catalog) - offset)) {
            break;
        }
        offset += (size_t)written;
        first_line = false;
    }

    if (offset == 0U) {
        strlcpy(catalog, "empty", sizeof(catalog));
    }

    return ble_manager_append_text(om, catalog);
}

static int ble_manager_append_navigation_state(struct os_mbuf *om)
{
    char payload[BOARD_NAV_PAYLOAD_MAX_LEN];

    snprintf(payload, sizeof(payload),
             "version=1\nactive=%u\nsequence=%" PRIu32 "\nsource=%s\ntitle=%s\ninstruction=%s\ndistance=%s\neta=%s\n",
             s_ble.navigation.active ? 1U : 0U,
             s_ble.navigation.sequence,
             s_ble.navigation.source,
             s_ble.navigation.title,
             s_ble.navigation.instruction,
             s_ble.navigation.distance,
             s_ble.navigation.eta);
    return ble_manager_append_text(om, payload);
}

static int ble_manager_handle_config_page_write(struct os_mbuf *om)
{
    char buffer[8];
    long requested_page;
    char *endptr = NULL;

    int rc = ble_manager_read_flat_text(om, buffer, sizeof(buffer));
    if (rc != 0) {
        return rc;
    }

    requested_page = strtol(buffer, &endptr, 10);
    if (endptr == buffer || requested_page < 0 || requested_page > 255L) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    s_ble.config_page = (uint8_t)requested_page;
    ble_manager_clamp_config_page();
    ble_manager_update_config_characteristics();
    return 0;
}

static int ble_manager_handle_config_toggle_write(struct os_mbuf *om)
{
    char buffer[BOARD_ANCS_APP_ID_MAX_LEN + 8U];
    char *separator;
    bool allowed;
    esp_err_t rc;

    int att_rc = ble_manager_read_flat_text(om, buffer, sizeof(buffer));
    if (att_rc != 0) {
        return att_rc;
    }

    separator = strchr(buffer, '|');
    if (separator == NULL || separator == buffer || separator[1] == '\0') {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    *separator = '\0';
    allowed = separator[1] == '1';

    if (strcmp(buffer, BLE_MANAGER_CALLS_PSEUDO_APP_ID) == 0) {
        rc = storage_manager_set_calls_allowed(allowed);
    } else {
        rc = storage_manager_set_notification_app_allowed(buffer, allowed);
    }
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "failed to update config for %s rc=%s", buffer, esp_err_to_name(rc));
        return BLE_ATT_ERR_UNLIKELY;
    }

    ble_manager_update_config_characteristics();
    ble_manager_emit_config_changed();
    ESP_LOGI(TAG, "config updated %s allowed=%d", buffer, allowed);
    return 0;
}

static bool ble_manager_text_is_true(const char *value)
{
    return value != NULL &&
           (strcmp(value, "1") == 0 ||
            strcmp(value, "true") == 0 ||
            strcmp(value, "yes") == 0 ||
            strcmp(value, "on") == 0);
}

static void ble_manager_trim_line(char *line)
{
    size_t len;

    if (line == NULL) {
        return;
    }

    len = strlen(line);
    while (len > 0U &&
           (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ' || line[len - 1] == '\t')) {
        line[len - 1] = '\0';
        len--;
    }
}

static int ble_manager_handle_navigation_write(struct os_mbuf *om)
{
    char buffer[BOARD_NAV_PAYLOAD_MAX_LEN];
    char *saveptr = NULL;
    char *line;
    ble_manager_navigation_state_t next = s_ble.navigation;
    bool touched = false;

    int att_rc = ble_manager_read_flat_text(om, buffer, sizeof(buffer));
    if (att_rc != 0) {
        return att_rc;
    }

    if (strcmp(buffer, "clear") == 0) {
        ble_manager_clear_navigation_state();
        ESP_LOGI(TAG, "navigation state cleared");
        return 0;
    }

    line = strtok_r(buffer, "\n", &saveptr);
    while (line != NULL) {
        char *separator;
        char *value;

        ble_manager_trim_line(line);
        separator = strchr(line, '=');
        if (separator != NULL && separator != line) {
            *separator = '\0';
            value = separator + 1;

            if (strcmp(line, "active") == 0) {
                next.active = ble_manager_text_is_true(value);
                touched = true;
            } else if (strcmp(line, "sequence") == 0) {
                next.sequence = (uint32_t)strtoul(value, NULL, 10);
                touched = true;
            } else if (strcmp(line, "source") == 0) {
                strlcpy(next.source, value, sizeof(next.source));
                touched = true;
            } else if (strcmp(line, "title") == 0) {
                strlcpy(next.title, value, sizeof(next.title));
                touched = true;
            } else if (strcmp(line, "instruction") == 0) {
                strlcpy(next.instruction, value, sizeof(next.instruction));
                touched = true;
            } else if (strcmp(line, "distance") == 0) {
                strlcpy(next.distance, value, sizeof(next.distance));
                touched = true;
            } else if (strcmp(line, "eta") == 0) {
                strlcpy(next.eta, value, sizeof(next.eta));
                touched = true;
            }
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    if (!touched) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (!next.active) {
        ble_manager_reset_navigation_state(&next);
    }

    ble_manager_apply_navigation_state(&next);
    ESP_LOGI(TAG, "navigation state updated active=%d seq=%" PRIu32 " source=%s",
             next.active, next.sequence, next.source);
    return 0;
}

static int ble_manager_config_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ble_manager_cfg_chr_t chr = (ble_manager_cfg_chr_t)(intptr_t)arg;

    (void)conn_handle;
    (void)attr_handle;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (chr == BLE_MANAGER_CFG_CHR_SUMMARY) {
            return ble_manager_append_config_summary(ctxt->om);
        }
        if (chr == BLE_MANAGER_CFG_CHR_PAGE) {
            char page_text[8];
            ble_manager_clamp_config_page();
            snprintf(page_text, sizeof(page_text), "%u", s_ble.config_page);
            return ble_manager_append_text(ctxt->om, page_text);
        }
        if (chr == BLE_MANAGER_CFG_CHR_CATALOG) {
            return ble_manager_append_config_catalog(ctxt->om);
        }
        if (chr == BLE_MANAGER_CFG_CHR_NAVIGATION) {
            return ble_manager_append_navigation_state(ctxt->om);
        }
        return BLE_ATT_ERR_UNLIKELY;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (chr == BLE_MANAGER_CFG_CHR_PAGE) {
            return ble_manager_handle_config_page_write(ctxt->om);
        }
        if (chr == BLE_MANAGER_CFG_CHR_TOGGLE) {
            return ble_manager_handle_config_toggle_write(ctxt->om);
        }
        if (chr == BLE_MANAGER_CFG_CHR_NAVIGATION) {
            return ble_manager_handle_navigation_write(ctxt->om);
        }
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static esp_err_t ble_manager_init_config_service(void)
{
    int rc;

    rc = ble_gatts_count_cfg(s_config_svcs);
    ESP_RETURN_ON_FALSE(rc == 0, ESP_FAIL, TAG, "ble_gatts_count_cfg failed: rc=%d", rc);

    rc = ble_gatts_add_svcs(s_config_svcs);
    ESP_RETURN_ON_FALSE(rc == 0, ESP_FAIL, TAG, "ble_gatts_add_svcs failed: rc=%d", rc);
    return ESP_OK;
}

static void ble_manager_update_config_characteristics(void)
{
    ble_manager_clamp_config_page();
    if (s_ble.cfg_summary_handle != 0U) {
        ble_gatts_chr_updated(s_ble.cfg_summary_handle);
    }
    if (s_ble.cfg_page_handle != 0U) {
        ble_gatts_chr_updated(s_ble.cfg_page_handle);
    }
    if (s_ble.cfg_catalog_handle != 0U) {
        ble_gatts_chr_updated(s_ble.cfg_catalog_handle);
    }
    ble_manager_notify_navigation_changed();
}

void ble_manager_notify_config_changed(void)
{
    ble_manager_update_config_characteristics();
    ble_manager_emit_config_changed();
}

static int ble_manager_load_bonded_peers(ble_addr_t *peers, int max_peers)
{
    int count = max_peers;
    int rc;

    if (peers == NULL || max_peers <= 0) {
        return 0;
    }

    rc = ble_store_util_bonded_peers(peers, &count, max_peers);
    if (rc != 0) {
        ESP_LOGW(TAG, "failed to load bonded peers, rc=%d", rc);
        return 0;
    }

    return count;
}

static int ble_manager_get_bonded_peer_count(void)
{
    ble_addr_t peers[BOARD_NOTIFICATION_QUEUE_MAX] = {0};
    return ble_manager_load_bonded_peers(peers, BOARD_NOTIFICATION_QUEUE_MAX);
}

static void ble_manager_restart_reconnect_timer(void)
{
    if (s_ble.reconnect_timer == NULL) {
        return;
    }

    if (esp_timer_is_active(s_ble.reconnect_timer)) {
        esp_timer_stop(s_ble.reconnect_timer);
    }

    if (esp_timer_start_once(s_ble.reconnect_timer, BOARD_BLE_RECONNECT_DELAY_MS * 1000ULL) != ESP_OK) {
        ESP_LOGW(TAG, "failed to start reconnect timer");
    }
}

static void ble_manager_emit_reconnect_sequence(void)
{
    ble_manager_emit_state(BLE_MANAGER_STATE_DISCONNECTED);
    ble_manager_emit_state(BLE_MANAGER_STATE_RECONNECTING);
    ble_manager_restart_reconnect_timer();
}

static void ble_manager_try_bond_recovery(const char *source, int status)
{
#if BOARD_BLE_BOND_RECOVERY_ON_SEC_FAIL
    if (!s_ble.had_bond_before_connect || s_ble.bond_recovery_done) {
        return;
    }

    ESP_LOGW(TAG, "stale bond suspected after %s failure status=%d, clearing local bonds", source, status);
    if (ble_manager_clear_bonds() == ESP_OK) {
        ble_manager_emit_bond(false);
        s_ble.had_bond_before_connect = false;
    }
    s_ble.bond_recovery_done = true;
#else
    (void)source;
    (void)status;
#endif
}

static esp_err_t ble_manager_start_general_advertising(void)
{
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};
    int rc;

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)s_ble.cfg.device_name;
    fields.name_len = (uint8_t)strlen(s_ble.cfg.device_name);
    fields.name_is_complete = 1;
    fields.sol_uuids128 = &s_ancs_service_uuid;
    fields.sol_num_uuids128 = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed, rc=%d", rc);
        return ESP_FAIL;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.filter_policy = BLE_HCI_ADV_FILT_NONE;

    rc = ble_gap_adv_start(s_ble.own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_manager_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed, rc=%d", rc);
        return ESP_FAIL;
    }

    s_ble.advertising = true;
    ESP_LOGI(TAG, "advertising open for manual or automatic reconnect");
    ble_manager_emit_state(BLE_MANAGER_STATE_ADVERTISING);
    return ESP_OK;
}

static esp_err_t ble_manager_start_advertising(void)
{
    struct ble_gap_adv_params adv_params = {0};
    ble_addr_t peers[BOARD_NOTIFICATION_QUEUE_MAX] = {0};
    int bonded_count = 0;
    int rc;

    if (!s_ble.synced) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_ble.connected || ble_gap_adv_active()) {
        return ESP_OK;
    }

    bonded_count = ble_manager_load_bonded_peers(peers, BOARD_NOTIFICATION_QUEUE_MAX);
    if (bonded_count > 0 && !s_ble.directed_adv_attempted) {
        memset(&adv_params, 0, sizeof(adv_params));
        adv_params.conn_mode = BLE_GAP_CONN_MODE_DIR;
        adv_params.high_duty_cycle = 0;

        rc = ble_gap_adv_start(s_ble.own_addr_type, &peers[0],
                               BOARD_BLE_DIRECTED_ADV_MS, &adv_params,
                               ble_manager_gap_event, NULL);
        if (rc == 0) {
            char addr_str[18];
            s_ble.advertising = true;
            s_ble.directed_adv_attempted = true;
            ESP_LOGI(TAG, "directed advertising to bonded peer %s for %u ms",
                     ble_manager_addr_to_str(&peers[0], addr_str, sizeof(addr_str)),
                     BOARD_BLE_DIRECTED_ADV_MS);
            ble_manager_emit_state(BLE_MANAGER_STATE_ADVERTISING);
            return ESP_OK;
        }

        ESP_LOGW(TAG, "directed advertising failed, rc=%d; falling back", rc);
        s_ble.directed_adv_attempted = true;
    }

    return ble_manager_start_general_advertising();
}

static void ble_manager_reconnect_timer_cb(void *arg)
{
    (void)arg;
    if (ble_manager_start_advertising() != ESP_OK) {
        ESP_LOGW(TAG, "failed to restart advertising from reconnect timer");
    }
}

static int ble_manager_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
    {
        int bonded_peers = 0;
        bool link_encrypted = false;
        bool link_bonded = false;
        int link_key_size = 0;

        ble_manager_emit_state(BLE_MANAGER_STATE_CONNECTING);
        if (event->connect.status != 0) {
            ESP_LOGW(TAG, "connect failed, status=%d", event->connect.status);
            s_ble.advertising = false;
            s_ble.connected = false;
            s_ble.secured = false;
            s_ble.conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_ble.had_bond_before_connect = false;
            ble_manager_emit_reconnect_sequence();
            return 0;
        }

        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        if (rc == 0) {
            char addr_str[18];
            link_encrypted = desc.sec_state.encrypted;
            link_bonded = desc.sec_state.bonded;
            link_key_size = desc.sec_state.key_size;
            ESP_LOGI(TAG, "connected to %s, encrypted=%d bonded=%d",
                     ble_manager_addr_to_str(&desc.peer_id_addr, addr_str, sizeof(addr_str)),
                     desc.sec_state.encrypted, desc.sec_state.bonded);
        }
        bonded_peers = ble_manager_get_bonded_peer_count();
        s_ble.had_bond_before_connect = bonded_peers > 0;
        s_ble.advertising = false;
        s_ble.connected = true;
        s_ble.secured = link_encrypted;
        s_ble.conn_handle = event->connect.conn_handle;
        s_ble.bond_recovery_done = false;
        s_ble.bond_just_established = false;
        ESP_LOGI(TAG, "bonded peers in store before security=%d", bonded_peers);
        ble_manager_emit_state(BLE_MANAGER_STATE_CONNECTED);

        if (link_encrypted) {
            ble_manager_handle_secure_link_ready(event->connect.conn_handle, link_bonded, link_key_size);
            return 0;
        }

        ESP_LOGI(TAG, "initiating security on conn_handle=%u (link not encrypted yet)",
                 event->connect.conn_handle);
        rc = ble_gap_security_initiate(event->connect.conn_handle);
        if (rc != 0) {
            ESP_LOGW(TAG, "ble_gap_security_initiate failed, rc=%d -> terminating", rc);
            ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
        return 0;
    }

    case BLE_GAP_EVENT_DISCONNECT:
        if (!s_ble.connected && s_ble.conn_handle == BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGW(TAG, "ignoring stale disconnect event, reason=%d", event->disconnect.reason);
            return 0;
        }
        ESP_LOGW(TAG, "disconnected, reason=%d", event->disconnect.reason);
        ancs_client_reset();
        cts_client_reset();
        ble_manager_clear_navigation_state();
        s_ble.advertising = false;
        s_ble.connected = false;
        s_ble.secured = false;
        s_ble.conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_ble.had_bond_before_connect = false;
        s_ble.directed_adv_attempted = false;
        s_ble.bond_just_established = false;
        ble_manager_emit_reconnect_sequence();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "advertising complete, reason=%d", event->adv_complete.reason);
        s_ble.advertising = false;
        if (!s_ble.connected) {
            if (ble_manager_start_advertising() != ESP_OK) {
                ESP_LOGW(TAG, "failed to continue advertising after completion");
            }
        }
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
    {
        bool bonded = false;
        int key_size = 0;

        ESP_LOGI(TAG, "ENC_CHANGE event: status=%d conn_handle=%u (current=%u connected=%d)",
                 event->enc_change.status, event->enc_change.conn_handle,
                 s_ble.conn_handle, s_ble.connected);

        if (!s_ble.connected || event->enc_change.conn_handle != s_ble.conn_handle) {
            ESP_LOGW(TAG, "ignoring stale enc_change event, status=%d", event->enc_change.status);
            return 0;
        }

        if (event->enc_change.status != 0) {
            ESP_LOGW(TAG, "enc_change failed, status=%d -> terminating", event->enc_change.status);
            ble_manager_try_bond_recovery("enc_change", event->enc_change.status);
            if (s_ble.connected && event->enc_change.conn_handle == s_ble.conn_handle) {
                ble_gap_terminate(event->enc_change.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            }
            return 0;
        }

        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        if (rc == 0) {
            bonded = desc.sec_state.bonded;
            key_size = desc.sec_state.key_size;
        }
        ble_manager_handle_secure_link_ready(event->enc_change.conn_handle, bonded, key_size);
        return 0;
    }

    case BLE_GAP_EVENT_NOTIFY_RX:
        return ancs_client_handle_notify_rx(event->notify_rx.conn_handle, event->notify_rx.attr_handle, event->notify_rx.om);

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU updated: %u", event->mtu.value);
        ancs_client_set_mtu(event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_ble.cfg_summary_handle ||
            event->subscribe.attr_handle == s_ble.cfg_page_handle ||
            event->subscribe.attr_handle == s_ble.cfg_catalog_handle ||
            event->subscribe.attr_handle == s_ble.cfg_navigation_handle) {
            ESP_LOGI(TAG, "config characteristic subscription handle=%u notify=%d",
                     event->subscribe.attr_handle,
                     event->subscribe.cur_notify);
        }
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        ESP_LOGW(TAG, "REPEAT_PAIRING event: conn_handle=%u (peer wants re-pair, deleting old bond)",
                 event->repeat_pairing.conn_handle);
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGW(TAG, "unexpected passkey action=%d on no-IO device", event->passkey.params.action);
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        ESP_LOGI(TAG, "connection updated, status=%d", event->conn_update.status);
        return 0;

    default:
        return 0;
    }
}

static void ble_manager_on_reset(int reason)
{
    ESP_LOGW(TAG, "nimble reset, reason=%d", reason);
}

static void ble_manager_on_sync(void)
{
    uint8_t addr_val[6] = {0};
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed, rc=%d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &s_ble.own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed, rc=%d", rc);
        return;
    }

    rc = ble_hs_id_copy_addr(s_ble.own_addr_type, addr_val, NULL);
    if (rc == 0) {
        ESP_LOGI(TAG, "device addr=%02X:%02X:%02X:%02X:%02X:%02X",
                 addr_val[5], addr_val[4], addr_val[3], addr_val[2], addr_val[1], addr_val[0]);
    }

    s_ble.synced = true;
    s_ble.connected = false;
    s_ble.secured = false;
    s_ble.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_ble.had_bond_before_connect = false;
    s_ble.bond_recovery_done = false;
    s_ble.directed_adv_attempted = false;
    s_ble.bond_just_established = false;
    ble_manager_emit_state(BLE_MANAGER_STATE_STACK_READY);
    ble_manager_emit_bond(ble_manager_has_bonded_peer());
    ble_manager_start_advertising();
}

static void ble_manager_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

const char *ble_manager_state_to_string(ble_manager_state_t state)
{
    switch (state) {
    case BLE_MANAGER_STATE_IDLE:
        return "IDLE";
    case BLE_MANAGER_STATE_STACK_READY:
        return "STACK_READY";
    case BLE_MANAGER_STATE_ADVERTISING:
        return "ADVERTISING";
    case BLE_MANAGER_STATE_CONNECTING:
        return "CONNECTING";
    case BLE_MANAGER_STATE_CONNECTED:
        return "CONNECTED";
    case BLE_MANAGER_STATE_SECURED:
        return "SECURED";
    case BLE_MANAGER_STATE_ANCS_READY:
        return "ANCS_READY";
    case BLE_MANAGER_STATE_DISCONNECTED:
        return "DISCONNECTED";
    case BLE_MANAGER_STATE_RECONNECTING:
        return "RECONNECTING";
    default:
        return "UNKNOWN";
    }
}

esp_err_t ble_manager_init(const ble_manager_config_t *config)
{
    esp_err_t ret;
    int rc;
    ancs_client_config_t ancs_cfg = {0};
    const esp_timer_create_args_t reconnect_timer_args = {
        .callback = ble_manager_reconnect_timer_cb,
        .name = "ble_reconnect",
    };

    ESP_RETURN_ON_FALSE(config != NULL && config->device_name != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid config");

    memset(&s_ble, 0, sizeof(s_ble));
    s_ble.cfg = *config;

    ret = nimble_port_init();
    ESP_RETURN_ON_ERROR(ret, TAG, "nimble_port_init failed");

    ble_svc_gap_init();
    ble_svc_gatt_init();
    rc = ble_svc_gap_device_name_set(config->device_name);
    ESP_RETURN_ON_FALSE(rc == 0, ESP_FAIL, TAG, "ble_svc_gap_device_name_set failed: rc=%d", rc);
    ESP_RETURN_ON_ERROR(ble_manager_init_config_service(), TAG, "config service init failed");

    ancs_cfg.ready_cb = ble_manager_ancs_ready_cb;
    ancs_cfg.notification_cb = ble_manager_ancs_notification_cb;
    ancs_cfg.user_ctx = NULL;
    ESP_RETURN_ON_ERROR(ancs_client_init(&ancs_cfg), TAG, "ancs_client_init failed");

    cts_client_config_t cts_cfg = {
        .time_cb = NULL,
        .user_ctx = NULL,
    };
    ESP_RETURN_ON_ERROR(cts_client_init(&cts_cfg), TAG, "cts_client_init failed");

    ble_hs_cfg.reset_cb = ble_manager_on_reset;
    ble_hs_cfg.sync_cb = ble_manager_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_store_config_init();

    ESP_RETURN_ON_ERROR(esp_timer_create(&reconnect_timer_args, &s_ble.reconnect_timer), TAG, "reconnect timer create failed");

    nimble_port_freertos_init(ble_manager_host_task);
    return ESP_OK;
}
