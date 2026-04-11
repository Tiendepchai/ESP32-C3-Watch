#include "ancs_client.h"

#include <inttypes.h>
#include <string.h>
#include "ancs_parser.h"
#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "nimble/ble.h"

static const char *TAG = BOARD_TAG_ANCS;

static const ble_uuid128_t s_ancs_service_uuid = BLE_UUID128_INIT(
    0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4,
    0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79);

static const ble_uuid128_t s_notification_source_uuid = BLE_UUID128_INIT(
    0xBD, 0x1D, 0xA2, 0x99, 0xE6, 0x25, 0x58, 0x8C,
    0xD9, 0x42, 0x01, 0x63, 0x0D, 0x12, 0xBF, 0x9F);

static const ble_uuid128_t s_control_point_uuid = BLE_UUID128_INIT(
    0xD9, 0xD9, 0xAA, 0xFD, 0xBD, 0x9B, 0x21, 0x98,
    0xA8, 0x49, 0xE1, 0x45, 0xF3, 0xD8, 0xD1, 0x69);

static const ble_uuid128_t s_data_source_uuid = BLE_UUID128_INIT(
    0xFB, 0x7B, 0x7C, 0xCE, 0x6A, 0xB3, 0x44, 0xBE,
    0xB5, 0x4B, 0xD6, 0x24, 0xE9, 0xC6, 0xEA, 0x22);

typedef struct {
    uint32_t ns_total;
    uint32_t ns_live;
    uint32_t ns_pre_existing;
    uint32_t ns_removed;
    uint32_t attr_requests_started;
    uint32_t attr_requests_completed;
    uint32_t attr_requests_retried;
    uint32_t attr_response_timeouts;
    uint32_t attr_send_failures;
    uint32_t attr_requests_dropped;
    uint32_t pre_existing_enriched;
    uint32_t pre_existing_skipped;
    uint32_t attr_queue_drops;
    uint32_t data_source_overflows;
    uint32_t attr_parse_failures;
} ancs_client_metrics_t;

typedef struct {
    uint16_t conn_handle;
    uint16_t mtu;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t notification_source_handle;
    uint16_t notification_source_cccd_handle;
    uint16_t data_source_handle;
    uint16_t data_source_cccd_handle;
    uint16_t control_point_handle;
    uint8_t data_source_buffer[BOARD_ANCS_DS_BUFFER_SIZE];
    size_t data_source_len;
    uint32_t pending_uid;
    notification_record_t pending_record;
    notification_record_t request_queue[BOARD_ANCS_ATTR_QUEUE_MAX];
    size_t request_queue_head;
    size_t request_queue_count;
    size_t pending_subscriptions;
    uint8_t discovery_retry_count;
    uint8_t pending_request_retry_count;
    uint8_t pre_existing_enrich_count;
    bool ready;
    bool attr_channel_ready;
    bool pending_record_valid;
    bool pending_record_discard;
    bool pending_request_awaiting_response;
    esp_timer_handle_t discovery_timer;
    esp_timer_handle_t attr_request_timer;
    ancs_client_metrics_t metrics;
    ancs_client_config_t cfg;
} ancs_client_ctx_t;

static ancs_client_ctx_t s_ancs;

static const ancs_attribute_request_t s_requested_attrs[] = {
    { .attribute_id = ANCS_ATTR_ID_APP_IDENTIFIER, .max_len = 0 },
    { .attribute_id = ANCS_ATTR_ID_TITLE, .max_len = BOARD_ANCS_TITLE_MAX_LEN - 1 },
    { .attribute_id = ANCS_ATTR_ID_MESSAGE, .max_len = BOARD_ANCS_MESSAGE_MAX_LEN - 1 },
};

static esp_err_t ancs_client_start_discovery_now(void);
static esp_err_t ancs_client_arm_discovery_timer(uint32_t delay_ms);
static esp_err_t ancs_client_arm_attr_request_timer(uint32_t delay_ms);
static void ancs_client_kick_attr_request(void);
static esp_err_t ancs_client_send_attr_request(const notification_record_t *record);
static void ancs_client_retry_pending_request(const char *reason);
static int ancs_client_cp_write_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                   struct ble_gatt_attr *attr, void *arg);
static void ancs_client_reset_metrics(void);
static void ancs_client_log_session_summary(const char *reason);

static void ancs_client_discovery_timer_cb(void *arg)
{
    struct ble_gap_conn_desc desc;
    (void)arg;

    if (s_ancs.conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }

    if (ble_gap_conn_find(s_ancs.conn_handle, &desc) != 0) {
        ESP_LOGW(TAG, "skip ANCS discovery, conn_handle=%u is no longer valid", s_ancs.conn_handle);
        return;
    }

    if (ancs_client_start_discovery_now() != ESP_OK) {
        ESP_LOGW(TAG, "failed to start scheduled ANCS discovery");
    }
}

static void ancs_client_clear_rx_state(void)
{
    s_ancs.data_source_len = 0;
    memset(s_ancs.data_source_buffer, 0, sizeof(s_ancs.data_source_buffer));
}

static void ancs_client_emit_ready(void)
{
    if (s_ancs.ready && s_ancs.cfg.ready_cb != NULL) {
        s_ancs.cfg.ready_cb(s_ancs.attr_channel_ready, s_ancs.cfg.user_ctx);
    }
}

static void ancs_client_reset_metrics(void)
{
    memset(&s_ancs.metrics, 0, sizeof(s_ancs.metrics));
    s_ancs.pre_existing_enrich_count = 0;
}

static void ancs_client_log_session_summary(const char *reason)
{
    if (s_ancs.metrics.ns_total == 0U &&
        s_ancs.metrics.attr_requests_started == 0U &&
        s_ancs.metrics.attr_requests_retried == 0U &&
        s_ancs.metrics.attr_send_failures == 0U &&
        s_ancs.metrics.data_source_overflows == 0U &&
        s_ancs.metrics.attr_parse_failures == 0U) {
        return;
    }

    ESP_LOGI(TAG,
             "session summary (%s): ns=%" PRIu32 " live=%" PRIu32 " pre=%" PRIu32 " removed=%" PRIu32
             " attrs[start=%" PRIu32 " done=%" PRIu32 " retry=%" PRIu32 " timeout=%" PRIu32
             " send_fail=%" PRIu32 " drop=%" PRIu32 "] pre_enrich=%" PRIu32 " pre_skip=%" PRIu32
             " qdrop=%" PRIu32 " ds_overflow=%" PRIu32 " parse_fail=%" PRIu32,
             reason != NULL ? reason : "unknown",
             s_ancs.metrics.ns_total,
             s_ancs.metrics.ns_live,
             s_ancs.metrics.ns_pre_existing,
             s_ancs.metrics.ns_removed,
             s_ancs.metrics.attr_requests_started,
             s_ancs.metrics.attr_requests_completed,
             s_ancs.metrics.attr_requests_retried,
             s_ancs.metrics.attr_response_timeouts,
             s_ancs.metrics.attr_send_failures,
             s_ancs.metrics.attr_requests_dropped,
             s_ancs.metrics.pre_existing_enriched,
             s_ancs.metrics.pre_existing_skipped,
             s_ancs.metrics.attr_queue_drops,
             s_ancs.metrics.data_source_overflows,
             s_ancs.metrics.attr_parse_failures);
}

static void ancs_client_clear_pending_request_state(void)
{
    if (s_ancs.attr_request_timer != NULL) {
        esp_timer_stop(s_ancs.attr_request_timer);
    }
    s_ancs.pending_uid = 0;
    s_ancs.pending_request_retry_count = 0;
    s_ancs.pending_record_valid = false;
    s_ancs.pending_record_discard = false;
    s_ancs.pending_request_awaiting_response = false;
    memset(&s_ancs.pending_record, 0, sizeof(s_ancs.pending_record));
}

static void ancs_client_clear_request_queue(void)
{
    s_ancs.request_queue_head = 0;
    s_ancs.request_queue_count = 0;
    memset(s_ancs.request_queue, 0, sizeof(s_ancs.request_queue));
}

static bool ancs_client_can_buffer_attr_requests(void)
{
    return s_ancs.control_point_handle != 0U &&
           s_ancs.data_source_handle != 0U &&
           s_ancs.data_source_cccd_handle != 0U;
}

static size_t ancs_client_request_queue_index(size_t offset)
{
    return (s_ancs.request_queue_head + offset) % BOARD_ANCS_ATTR_QUEUE_MAX;
}

static bool ancs_client_peek_queued_record(notification_record_t *out_record)
{
    if (out_record == NULL || s_ancs.request_queue_count == 0U) {
        return false;
    }

    *out_record = s_ancs.request_queue[s_ancs.request_queue_head];
    return true;
}

static void ancs_client_pop_queued_record(void)
{
    if (s_ancs.request_queue_count == 0U) {
        return;
    }

    memset(&s_ancs.request_queue[s_ancs.request_queue_head], 0,
           sizeof(s_ancs.request_queue[s_ancs.request_queue_head]));
    s_ancs.request_queue_head = (s_ancs.request_queue_head + 1U) % BOARD_ANCS_ATTR_QUEUE_MAX;
    s_ancs.request_queue_count--;
}

static void ancs_client_remove_queued_record(uint32_t uid)
{
    if (uid == 0U || s_ancs.request_queue_count == 0U) {
        return;
    }

    for (size_t offset = 0; offset < s_ancs.request_queue_count; ++offset) {
        size_t index = ancs_client_request_queue_index(offset);

        if (s_ancs.request_queue[index].uid != uid) {
            continue;
        }

        for (size_t move = offset; move + 1U < s_ancs.request_queue_count; ++move) {
            size_t dst = ancs_client_request_queue_index(move);
            size_t src = ancs_client_request_queue_index(move + 1U);
            s_ancs.request_queue[dst] = s_ancs.request_queue[src];
        }

        memset(&s_ancs.request_queue[ancs_client_request_queue_index(s_ancs.request_queue_count - 1U)], 0,
               sizeof(s_ancs.request_queue[0]));
        s_ancs.request_queue_count--;
        return;
    }
}

static void ancs_client_move_queued_record_to_front(size_t offset, const notification_record_t *record)
{
    notification_record_t queued_record;

    if (record == NULL || offset >= s_ancs.request_queue_count) {
        return;
    }

    queued_record = *record;
    while (offset > 0U) {
        size_t dst = ancs_client_request_queue_index(offset);
        size_t src = ancs_client_request_queue_index(offset - 1U);
        s_ancs.request_queue[dst] = s_ancs.request_queue[src];
        offset--;
    }
    s_ancs.request_queue[s_ancs.request_queue_head] = queued_record;
}

static void ancs_client_queue_record(const notification_record_t *record, bool prioritize)
{
    size_t insert_index;

    if (record == NULL || record->uid == 0U) {
        return;
    }

    if (s_ancs.pending_uid == record->uid) {
        s_ancs.pending_record = *record;
        s_ancs.pending_record_valid = true;
        return;
    }

    for (size_t offset = 0; offset < s_ancs.request_queue_count; ++offset) {
        size_t index = ancs_client_request_queue_index(offset);

        if (s_ancs.request_queue[index].uid == record->uid) {
            if (prioritize && offset > 0U) {
                ancs_client_move_queued_record_to_front(offset, record);
            } else {
                s_ancs.request_queue[index] = *record;
            }
            return;
        }
    }

    if (s_ancs.request_queue_count == BOARD_ANCS_ATTR_QUEUE_MAX) {
        s_ancs.metrics.attr_queue_drops++;
        ESP_LOGW(TAG, "ANCS attr queue full, dropping oldest queued uid=%" PRIu32,
                 s_ancs.request_queue[s_ancs.request_queue_head].uid);
        ancs_client_pop_queued_record();
    }

    if (prioritize) {
        s_ancs.request_queue_head =
            (s_ancs.request_queue_head + BOARD_ANCS_ATTR_QUEUE_MAX - 1U) % BOARD_ANCS_ATTR_QUEUE_MAX;
        s_ancs.request_queue[s_ancs.request_queue_head] = *record;
    } else {
        insert_index = ancs_client_request_queue_index(s_ancs.request_queue_count);
        s_ancs.request_queue[insert_index] = *record;
    }
    s_ancs.request_queue_count++;
}

static void ancs_client_cancel_record(uint32_t uid)
{
    if (uid == 0U) {
        return;
    }

    ancs_client_remove_queued_record(uid);

    if (s_ancs.pending_uid == uid) {
        s_ancs.pending_record_discard = true;
    }
}

static void ancs_client_emit_notification(const notification_record_t *record, bool complete)
{
    if (record != NULL && s_ancs.cfg.notification_cb != NULL) {
        s_ancs.cfg.notification_cb(record, complete, s_ancs.cfg.user_ctx);
    }
}

static esp_err_t ancs_client_arm_attr_request_timer(uint32_t delay_ms)
{
    if (s_ancs.attr_request_timer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_timer_stop(s_ancs.attr_request_timer);
    return esp_timer_start_once(s_ancs.attr_request_timer, (uint64_t)delay_ms * 1000ULL);
}

static esp_err_t ancs_client_send_attr_request(const notification_record_t *record)
{
    uint8_t cmd[32];
    size_t cmd_len;
    int rc;

    ESP_RETURN_ON_FALSE(record != NULL && record->uid != 0U, ESP_ERR_INVALID_ARG, TAG, "invalid record");
    ESP_RETURN_ON_FALSE(s_ancs.conn_handle != BLE_HS_CONN_HANDLE_NONE, ESP_ERR_INVALID_STATE, TAG,
                        "ANCS request without active connection");
    ESP_RETURN_ON_FALSE(s_ancs.control_point_handle != 0U, ESP_ERR_INVALID_STATE, TAG,
                        "ANCS control point not ready");

    cmd_len = ancs_build_get_notification_attrs_cmd(cmd, sizeof(cmd), record->uid,
                                                    s_requested_attrs,
                                                    sizeof(s_requested_attrs) / sizeof(s_requested_attrs[0]));
    ESP_RETURN_ON_FALSE(cmd_len != 0U, ESP_FAIL, TAG, "failed to build control point command");

    rc = ble_gattc_write_flat(s_ancs.conn_handle, s_ancs.control_point_handle,
                              cmd, cmd_len, ancs_client_cp_write_cb, NULL);
    ESP_RETURN_ON_FALSE(rc == 0, ESP_FAIL, TAG, "failed to request attributes, rc=%d", rc);
    s_ancs.metrics.attr_requests_started++;
    return ESP_OK;
}

static void ancs_client_retry_pending_request(const char *reason)
{
    esp_err_t rc;

    if (!s_ancs.pending_record_valid || s_ancs.pending_uid == 0U) {
        return;
    }

    if (s_ancs.pending_request_retry_count >= BOARD_ANCS_ATTR_MAX_RETRIES) {
        s_ancs.metrics.attr_requests_dropped++;
        ESP_LOGW(TAG, "dropping ANCS attr request uid=%" PRIu32 " after %u retries (%s)",
                 s_ancs.pending_uid,
                 s_ancs.pending_request_retry_count,
                 reason);
        ancs_client_clear_rx_state();
        ancs_client_clear_pending_request_state();
        ancs_client_kick_attr_request();
        return;
    }

    s_ancs.pending_request_retry_count++;
    s_ancs.metrics.attr_requests_retried++;
    s_ancs.pending_request_awaiting_response = false;
    ancs_client_clear_rx_state();
    rc = ancs_client_send_attr_request(&s_ancs.pending_record);
    if (rc == ESP_OK) {
        s_ancs.pending_request_awaiting_response = true;
        ESP_LOGW(TAG, "retrying ANCS attr request uid=%" PRIu32 " attempt %u/%u (%s)",
                 s_ancs.pending_uid,
                 s_ancs.pending_request_retry_count,
                 BOARD_ANCS_ATTR_MAX_RETRIES,
                 reason);
        if (ancs_client_arm_attr_request_timer(BOARD_ANCS_ATTR_REQUEST_TIMEOUT_MS) != ESP_OK) {
            ESP_LOGW(TAG, "failed to arm ANCS attr timeout after retry");
        }
        return;
    }

    ESP_LOGW(TAG, "retry send failed for uid=%" PRIu32 " attempt %u/%u (%s), rc=%s",
             s_ancs.pending_uid,
             s_ancs.pending_request_retry_count,
             BOARD_ANCS_ATTR_MAX_RETRIES,
             reason,
             esp_err_to_name(rc));

    if (s_ancs.pending_request_retry_count >= BOARD_ANCS_ATTR_MAX_RETRIES) {
        ancs_client_clear_pending_request_state();
        ancs_client_kick_attr_request();
        return;
    }

    if (ancs_client_arm_attr_request_timer(BOARD_ANCS_ATTR_RETRY_DELAY_MS) != ESP_OK) {
        ESP_LOGW(TAG, "failed to arm ANCS attr retry timer");
        ancs_client_clear_pending_request_state();
        ancs_client_kick_attr_request();
    }
}

static void ancs_client_attr_request_timer_cb(void *arg)
{
    const char *reason;
    (void)arg;

    if (!s_ancs.pending_record_valid || s_ancs.pending_uid == 0U) {
        return;
    }

    reason = s_ancs.pending_request_awaiting_response ? "response timeout" : "scheduled resend";
    if (s_ancs.pending_request_awaiting_response) {
        s_ancs.metrics.attr_response_timeouts++;
    }
    ancs_client_retry_pending_request(reason);
}

static int ancs_client_cp_write_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                   struct ble_gatt_attr *attr, void *arg)
{
    (void)conn_handle;
    (void)attr;
    (void)arg;

    if (error->status != 0) {
        s_ancs.metrics.attr_send_failures++;
        ESP_LOGW(TAG, "control point write failed for uid=%" PRIu32 ", status=%d",
                 s_ancs.pending_uid, error->status);
        ancs_client_retry_pending_request("control point write failed");
    }
    return 0;
}

static void ancs_client_kick_attr_request(void)
{
    esp_err_t rc;
    notification_record_t queued_record = {0};

    if (!s_ancs.attr_channel_ready || s_ancs.pending_uid != 0U) {
        return;
    }
    if (!ancs_client_peek_queued_record(&queued_record)) {
        return;
    }

    ancs_client_pop_queued_record();
    s_ancs.pending_uid = queued_record.uid;
    s_ancs.pending_record = queued_record;
    s_ancs.pending_record_valid = true;
    s_ancs.pending_record_discard = false;
    s_ancs.pending_request_retry_count = 0;
    s_ancs.pending_request_awaiting_response = false;
    ancs_client_clear_rx_state();

    rc = ancs_client_send_attr_request(&s_ancs.pending_record);
    if (rc == ESP_OK) {
        s_ancs.pending_request_awaiting_response = true;
        if (ancs_client_arm_attr_request_timer(BOARD_ANCS_ATTR_REQUEST_TIMEOUT_MS) != ESP_OK) {
            ESP_LOGW(TAG, "failed to arm ANCS attr timeout");
        }
        return;
    }

    ESP_LOGW(TAG, "initial ANCS attr request failed for uid=%" PRIu32 ", rc=%s",
             s_ancs.pending_uid, esp_err_to_name(rc));
    s_ancs.metrics.attr_send_failures++;
    if (BOARD_ANCS_ATTR_MAX_RETRIES == 0U ||
        ancs_client_arm_attr_request_timer(BOARD_ANCS_ATTR_RETRY_DELAY_MS) != ESP_OK) {
        ancs_client_clear_pending_request_state();
        ancs_client_kick_attr_request();
    }
}

static int ancs_client_subscribe_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                    struct ble_gatt_attr *attr, void *arg)
{
    (void)conn_handle;
    (void)arg;

    if (error->status != 0) {
        ESP_LOGW(TAG, "CCCD write failed, handle=%u status=%d", attr != NULL ? attr->handle : 0U, error->status);
    }

    if (s_ancs.pending_subscriptions > 0U) {
        s_ancs.pending_subscriptions--;
    }

    if (s_ancs.pending_subscriptions == 0U) {
        s_ancs.ready = true;
        s_ancs.attr_channel_ready = (s_ancs.control_point_handle != 0U &&
                                     s_ancs.data_source_handle != 0U &&
                                     s_ancs.data_source_cccd_handle != 0U);
        ESP_LOGI(TAG, "ANCS ready, attrs=%s queued=%u",
                 s_ancs.attr_channel_ready ? "enabled" : "basic-only",
                 (unsigned)s_ancs.request_queue_count);
        ancs_client_emit_ready();
        ancs_client_kick_attr_request();
    }

    return 0;
}

static esp_err_t ancs_client_start_subscriptions(void)
{
    static const uint8_t cccd_enable_notify[2] = { 0x01, 0x00 };
    int rc;

    s_ancs.pending_subscriptions = 0;

    if (s_ancs.notification_source_cccd_handle != 0U) {
        s_ancs.pending_subscriptions++;
        rc = ble_gattc_write_flat(s_ancs.conn_handle, s_ancs.notification_source_cccd_handle,
                                  cccd_enable_notify, sizeof(cccd_enable_notify),
                                  ancs_client_subscribe_cb, NULL);
        if (rc != 0) {
            ESP_LOGW(TAG, "failed to subscribe Notification Source, rc=%d", rc);
            s_ancs.pending_subscriptions--;
        }
    }

    if (s_ancs.data_source_cccd_handle != 0U) {
        s_ancs.pending_subscriptions++;
        rc = ble_gattc_write_flat(s_ancs.conn_handle, s_ancs.data_source_cccd_handle,
                                  cccd_enable_notify, sizeof(cccd_enable_notify),
                                  ancs_client_subscribe_cb, NULL);
        if (rc != 0) {
            ESP_LOGW(TAG, "failed to subscribe Data Source, rc=%d", rc);
            s_ancs.pending_subscriptions--;
        }
    }

    if (s_ancs.pending_subscriptions == 0U) {
        s_ancs.ready = true;
        s_ancs.attr_channel_ready = false;
        ancs_client_emit_ready();
    }

    return ESP_OK;
}

static int ancs_client_disc_dsc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                   uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg)
{
    (void)conn_handle;
    (void)chr_val_handle;
    (void)arg;

    if (error->status == 0 && dsc != NULL) {
        if (ble_uuid_cmp(&dsc->uuid.u, BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16)) == 0) {
            if (s_ancs.notification_source_handle != 0U &&
                dsc->handle > s_ancs.notification_source_handle &&
                (s_ancs.data_source_handle == 0U || dsc->handle < s_ancs.data_source_handle) &&
                s_ancs.notification_source_cccd_handle == 0U) {
                s_ancs.notification_source_cccd_handle = dsc->handle;
            } else if (s_ancs.data_source_handle != 0U &&
                       dsc->handle > s_ancs.data_source_handle &&
                       s_ancs.data_source_cccd_handle == 0U) {
                s_ancs.data_source_cccd_handle = dsc->handle;
            }
        }
        return 0;
    }

    if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "descriptor discovery done, ns_cccd=%u ds_cccd=%u",
                 s_ancs.notification_source_cccd_handle, s_ancs.data_source_cccd_handle);
        ancs_client_start_subscriptions();
        return 0;
    }

    ESP_LOGW(TAG, "descriptor discovery failed, status=%d", error->status);
    return 0;
}

static int ancs_client_disc_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                   const struct ble_gatt_chr *chr, void *arg)
{
    int rc;
    (void)arg;

    if (error->status == 0 && chr != NULL) {
        if ((chr->properties & BLE_GATT_CHR_PROP_NOTIFY) &&
            ble_uuid_cmp(&chr->uuid.u, &s_notification_source_uuid.u) == 0) {
            s_ancs.notification_source_handle = chr->val_handle;
            ESP_LOGI(TAG, "Notification Source handle=%u", s_ancs.notification_source_handle);
        } else if ((chr->properties & BLE_GATT_CHR_PROP_NOTIFY) &&
                   ble_uuid_cmp(&chr->uuid.u, &s_data_source_uuid.u) == 0) {
            s_ancs.data_source_handle = chr->val_handle;
            ESP_LOGI(TAG, "Data Source handle=%u", s_ancs.data_source_handle);
        } else if ((chr->properties & BLE_GATT_CHR_PROP_WRITE) &&
                   ble_uuid_cmp(&chr->uuid.u, &s_control_point_uuid.u) == 0) {
            s_ancs.control_point_handle = chr->val_handle;
            ESP_LOGI(TAG, "Control Point handle=%u", s_ancs.control_point_handle);
        }
        return 0;
    }

    if (error->status == BLE_HS_EDONE) {
        if (s_ancs.notification_source_handle == 0U) {
            ESP_LOGE(TAG, "ANCS Notification Source characteristic not found");
            return 0;
        }

        rc = ble_gattc_disc_all_dscs(conn_handle, s_ancs.service_start_handle,
                                     s_ancs.service_end_handle, ancs_client_disc_dsc_cb, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "failed to discover ANCS descriptors, rc=%d", rc);
        }
        return 0;
    }

    ESP_LOGW(TAG, "characteristic discovery failed, status=%d", error->status);
    return 0;
}

static int ancs_client_disc_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                   const struct ble_gatt_svc *svc, void *arg)
{
    int rc;
    (void)arg;

    if (error->status == 0 && svc != NULL) {
        s_ancs.service_start_handle = svc->start_handle;
        s_ancs.service_end_handle = svc->end_handle;
        ESP_LOGI(TAG, "ANCS service found: start=%u end=%u",
                 s_ancs.service_start_handle, s_ancs.service_end_handle);
        return 0;
    }

    if (error->status == BLE_HS_EDONE) {
        if (s_ancs.service_start_handle == 0U || s_ancs.service_end_handle == 0U) {
            if (s_ancs.discovery_retry_count < BOARD_ANCS_DISCOVERY_MAX_RETRIES) {
                s_ancs.discovery_retry_count++;
                ESP_LOGW(TAG, "ANCS service not published on peer, retry %u/%u in %u ms",
                         s_ancs.discovery_retry_count,
                         BOARD_ANCS_DISCOVERY_MAX_RETRIES,
                         BOARD_ANCS_DISCOVERY_RETRY_MS);
                ancs_client_arm_discovery_timer(BOARD_ANCS_DISCOVERY_RETRY_MS);
            } else {
                ESP_LOGW(TAG, "ANCS service not published on peer after %u retries",
                         BOARD_ANCS_DISCOVERY_MAX_RETRIES);
            }
            return 0;
        }

        rc = ble_gattc_disc_all_chrs(conn_handle, s_ancs.service_start_handle,
                                     s_ancs.service_end_handle, ancs_client_disc_chr_cb, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "failed to discover ANCS chars, rc=%d", rc);
        }
        return 0;
    }

    if (error->status == BLE_HS_ENOTCONN) {
        ESP_LOGW(TAG, "service discovery aborted because peer disconnected");
    } else {
        ESP_LOGW(TAG, "service discovery failed, status=%d", error->status);
    }
    return 0;
}

esp_err_t ancs_client_init(const ancs_client_config_t *config)
{
    esp_timer_handle_t existing_timer = s_ancs.discovery_timer;
    esp_timer_handle_t existing_attr_timer = s_ancs.attr_request_timer;
    const esp_timer_create_args_t discovery_timer_args = {
        .callback = ancs_client_discovery_timer_cb,
        .name = "ancs_disc",
    };
    const esp_timer_create_args_t attr_timer_args = {
        .callback = ancs_client_attr_request_timer_cb,
        .name = "ancs_attr",
    };

    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    memset(&s_ancs, 0, sizeof(s_ancs));
    s_ancs.discovery_timer = existing_timer;
    s_ancs.attr_request_timer = existing_attr_timer;
    s_ancs.cfg = *config;
    s_ancs.mtu = 23;
    s_ancs.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    ancs_client_clear_request_queue();

    if (s_ancs.discovery_timer == NULL) {
        ESP_RETURN_ON_ERROR(esp_timer_create(&discovery_timer_args, &s_ancs.discovery_timer), TAG,
                            "failed to create ANCS discovery timer");
    }
    if (s_ancs.attr_request_timer == NULL) {
        ESP_RETURN_ON_ERROR(esp_timer_create(&attr_timer_args, &s_ancs.attr_request_timer), TAG,
                            "failed to create ANCS attr timer");
    }
    ancs_client_reset_metrics();
    return ESP_OK;
}

void ancs_client_reset(void)
{
    ancs_client_config_t cfg = s_ancs.cfg;
    esp_timer_handle_t discovery_timer = s_ancs.discovery_timer;
    esp_timer_handle_t attr_request_timer = s_ancs.attr_request_timer;

    if (discovery_timer != NULL) {
        esp_timer_stop(discovery_timer);
    }
    if (attr_request_timer != NULL) {
        esp_timer_stop(attr_request_timer);
    }

    ancs_client_log_session_summary("reset");

    memset(&s_ancs, 0, sizeof(s_ancs));
    s_ancs.discovery_timer = discovery_timer;
    s_ancs.attr_request_timer = attr_request_timer;
    s_ancs.cfg = cfg;
    s_ancs.mtu = 23;
    s_ancs.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    ancs_client_clear_request_queue();
    ancs_client_reset_metrics();
}

static esp_err_t ancs_client_arm_discovery_timer(uint32_t delay_ms)
{
    if (s_ancs.discovery_timer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_timer_stop(s_ancs.discovery_timer);
    if (delay_ms == 0U) {
        return ancs_client_start_discovery_now();
    }

    return esp_timer_start_once(s_ancs.discovery_timer, (uint64_t)delay_ms * 1000ULL);
}

static esp_err_t ancs_client_start_discovery_now(void)
{
    int rc;

    s_ancs.service_start_handle = 0;
    s_ancs.service_end_handle = 0;
    s_ancs.notification_source_handle = 0;
    s_ancs.notification_source_cccd_handle = 0;
    s_ancs.data_source_handle = 0;
    s_ancs.data_source_cccd_handle = 0;
    s_ancs.control_point_handle = 0;
    s_ancs.ready = false;
    s_ancs.attr_channel_ready = false;
    ancs_client_clear_rx_state();
    ancs_client_clear_pending_request_state();
    ancs_client_clear_request_queue();
    ancs_client_reset_metrics();

    ESP_LOGI(TAG, "starting ANCS service discovery, conn_handle=%u retry=%u",
             s_ancs.conn_handle, s_ancs.discovery_retry_count);

    rc = ble_gattc_disc_svc_by_uuid(s_ancs.conn_handle, &s_ancs_service_uuid.u, ancs_client_disc_svc_cb, NULL);
    ESP_RETURN_ON_FALSE(rc == 0, ESP_FAIL, TAG, "ble_gattc_disc_svc_by_uuid failed: rc=%d", rc);
    return ESP_OK;
}

esp_err_t ancs_client_start_discovery(uint16_t conn_handle)
{
    s_ancs.conn_handle = conn_handle;
    s_ancs.discovery_retry_count = 0;
    return ancs_client_start_discovery_now();
}

esp_err_t ancs_client_schedule_discovery(uint16_t conn_handle, uint32_t delay_ms)
{
    s_ancs.conn_handle = conn_handle;
    s_ancs.discovery_retry_count = 0;
    ESP_LOGI(TAG, "ANCS discovery scheduled in %u ms", delay_ms);
    return ancs_client_arm_discovery_timer(delay_ms);
}

void ancs_client_set_mtu(uint16_t mtu)
{
    s_ancs.mtu = mtu;
}

bool ancs_client_is_ready(void)
{
    return s_ancs.ready;
}

bool ancs_client_is_attr_channel_ready(void)
{
    return s_ancs.attr_channel_ready;
}

esp_err_t ancs_client_request_notification_details(const notification_record_t *record, bool prioritize)
{
    ESP_RETURN_ON_FALSE(record != NULL && record->uid != 0U, ESP_ERR_INVALID_ARG, TAG, "invalid record");
    ESP_RETURN_ON_FALSE(s_ancs.conn_handle != BLE_HS_CONN_HANDLE_NONE, ESP_ERR_INVALID_STATE, TAG,
                        "ANCS detail request without active connection");
    ESP_RETURN_ON_FALSE(ancs_client_can_buffer_attr_requests(), ESP_ERR_INVALID_STATE, TAG,
                        "ANCS detail request before attr channel handles");

    if (record->details_complete) {
        return ESP_OK;
    }

    ancs_client_queue_record(record, prioritize);
    if (s_ancs.attr_channel_ready) {
        ancs_client_kick_attr_request();
    }
    return ESP_OK;
}

int ancs_client_handle_notify_rx(uint16_t conn_handle, uint16_t attr_handle, const struct os_mbuf *om)
{
    uint8_t buffer[BOARD_ANCS_DS_BUFFER_SIZE];
    notification_record_t record = {0};
    ancs_notification_source_event_t event = {0};
    int payload_len;
    bool buffer_attrs;
    bool pre_existing;

    if (om == NULL) {
        return 0;
    }

    payload_len = OS_MBUF_PKTLEN(om);
    if (payload_len <= 0 || payload_len > (int)sizeof(buffer)) {
        ESP_LOGW(TAG, "invalid notify payload length=%d", payload_len);
        return 0;
    }

    if (os_mbuf_copydata(om, 0, payload_len, buffer) != 0) {
        ESP_LOGW(TAG, "failed to copy notify payload");
        return 0;
    }

    if (attr_handle == s_ancs.notification_source_handle) {
        if (ancs_parse_notification_source(buffer, payload_len, &event) != ESP_OK) {
            ESP_LOGW(TAG, "failed to parse Notification Source payload");
            return 0;
        }

        record.uid = event.notification_uid;
        record.event_id = event.event_id;
        record.event_flags = event.event_flags;
        record.category_id = event.category_id;
        record.category_count = event.category_count;
        record.timestamp_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);
        record.details_complete = false;
        record.valid = true;
        buffer_attrs = ancs_client_can_buffer_attr_requests();
        pre_existing = (event.event_flags & ANCS_EVENT_FLAG_PRE_EXISTING) != 0U;
        s_ancs.metrics.ns_total++;
        if (pre_existing) {
            s_ancs.metrics.ns_pre_existing++;
        } else {
            s_ancs.metrics.ns_live++;
        }
        if (event.event_id == ANCS_EVENT_ID_NOTIFICATION_REMOVED) {
            s_ancs.metrics.ns_removed++;
        }

        if (pre_existing) {
            ESP_LOGD(TAG, "NS replay %s category=%s uid=%" PRIu32,
                     ancs_event_id_to_string(event.event_id),
                     ancs_category_id_to_string(event.category_id),
                     event.notification_uid);
        } else {
            ESP_LOGI(TAG, "NS %s category=%s uid=%" PRIu32,
                     ancs_event_id_to_string(event.event_id),
                     ancs_category_id_to_string(event.category_id),
                     event.notification_uid);
        }

        ancs_client_emit_notification(&record, false);

        if (event.event_id == ANCS_EVENT_ID_NOTIFICATION_REMOVED) {
            ancs_client_cancel_record(event.notification_uid);
            return 0;
        }

        if (buffer_attrs && !pre_existing) {
            ancs_client_queue_record(&record, false);
            if (s_ancs.attr_channel_ready) {
                ancs_client_kick_attr_request();
            }
        } else if (buffer_attrs && pre_existing) {
            s_ancs.metrics.pre_existing_skipped++;
            ESP_LOGD(TAG, "deferring pre-existing attr fetch uid=%" PRIu32 " to selection window",
                     event.notification_uid);
        }
        return 0;
    }

    if (attr_handle == s_ancs.data_source_handle) {
        if (s_ancs.data_source_len + (size_t)payload_len > sizeof(s_ancs.data_source_buffer)) {
            s_ancs.metrics.data_source_overflows++;
            ESP_LOGW(TAG, "Data Source buffer overflow, dropping current response uid=%" PRIu32,
                     s_ancs.pending_uid);
            ancs_client_clear_rx_state();
            ancs_client_clear_pending_request_state();
            ancs_client_kick_attr_request();
            return 0;
        }

        memcpy(&s_ancs.data_source_buffer[s_ancs.data_source_len], buffer, (size_t)payload_len);
        s_ancs.data_source_len += (size_t)payload_len;
        if (s_ancs.pending_uid != 0U) {
            s_ancs.pending_request_awaiting_response = true;
            if (ancs_client_arm_attr_request_timer(BOARD_ANCS_ATTR_REQUEST_TIMEOUT_MS) != ESP_OK) {
                ESP_LOGW(TAG, "failed to refresh ANCS attr timeout");
            }
        }

        if (ancs_is_notification_attrs_complete(s_ancs.data_source_buffer, s_ancs.data_source_len,
                                                s_requested_attrs,
                                                sizeof(s_requested_attrs) / sizeof(s_requested_attrs[0]))) {
            if (ancs_parse_notification_attrs(s_ancs.data_source_buffer, s_ancs.data_source_len, &record) == ESP_OK) {
                if (s_ancs.pending_record_valid && s_ancs.pending_record.uid == record.uid) {
                    record.event_id = s_ancs.pending_record.event_id;
                    record.event_flags = s_ancs.pending_record.event_flags;
                    record.category_id = s_ancs.pending_record.category_id;
                    record.category_count = s_ancs.pending_record.category_count;
                    record.timestamp_ms = s_ancs.pending_record.timestamp_ms;
                }
                if (record.timestamp_ms == 0U) {
                    record.timestamp_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);
                }
                record.details_complete = true;
                record.valid = true;
                s_ancs.metrics.attr_requests_completed++;
                if (!s_ancs.pending_record_discard) {
                    ancs_client_emit_notification(&record, true);
                }
            } else {
                s_ancs.metrics.attr_parse_failures++;
                ESP_LOGW(TAG, "failed to parse complete Data Source response");
            }

            ancs_client_clear_rx_state();
            ancs_client_clear_pending_request_state();
            ancs_client_kick_attr_request();
        }
        return 0;
    }

    ESP_LOGD(TAG, "notify received on unrelated handle=%u conn=%u", attr_handle, conn_handle);
    return 0;
}
