#include "cts_client.h"

#include <string.h>
#include <time.h>
#include "board_config.h"
#include "esp_log.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "os/os_mbuf.h"

#define CTS_SVC_UUID         0x1805
#define CTS_CHR_CURTIME_UUID 0x2A2B

static const char *TAG = "cts_client";

typedef struct {
    cts_client_config_t cfg;
    uint16_t conn_handle;
    bool synced;
    bool in_progress;
} cts_ctx_t;

static cts_ctx_t s_cts;

static int cts_on_read(uint16_t conn_handle, const struct ble_gatt_error *error,
                       struct ble_gatt_attr *attr, void *arg);
static int cts_on_chr_disc(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr, void *arg);
static int cts_on_svc_disc(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *svc, void *arg);

esp_err_t cts_client_init(const cts_client_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(&s_cts, 0, sizeof(s_cts));
    s_cts.cfg = *config;
    s_cts.conn_handle = 0xFFFF;
    ESP_LOGI(TAG, "cts client initialized");
    return ESP_OK;
}

void cts_client_reset(void)
{
    s_cts.conn_handle = 0xFFFF;
    s_cts.in_progress = false;
    /* Keep synced=true across disconnect — system time is still valid. */
}

bool cts_client_is_time_synced(void)
{
    return s_cts.synced;
}

esp_err_t cts_client_start_discovery(uint16_t conn_handle)
{
    if (s_cts.in_progress) {
        ESP_LOGI(TAG, "CTS discovery already in progress");
        return ESP_OK;
    }
    s_cts.conn_handle = conn_handle;
    s_cts.in_progress = true;

    ble_uuid16_t svc_uuid = BLE_UUID16_INIT(CTS_SVC_UUID);
    int rc = ble_gattc_disc_svc_by_uuid(conn_handle, &svc_uuid.u, cts_on_svc_disc, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_gattc_disc_svc_by_uuid failed rc=%d", rc);
        s_cts.in_progress = false;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "CTS discovery started, conn_handle=%u", conn_handle);
    return ESP_OK;
}

static int cts_on_svc_disc(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *svc, void *arg)
{
    (void)arg;
    if (conn_handle != s_cts.conn_handle) {
        return 0;
    }
    if (error->status == BLE_HS_EDONE) {
        if (svc == NULL) {
            ESP_LOGI(TAG, "CTS service not present on peer");
            s_cts.in_progress = false;
        }
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGW(TAG, "CTS svc disc error=%d", error->status);
        s_cts.in_progress = false;
        return 0;
    }
    if (svc == NULL) {
        return 0;
    }

    ble_uuid16_t chr_uuid = BLE_UUID16_INIT(CTS_CHR_CURTIME_UUID);
    int rc = ble_gattc_disc_chrs_by_uuid(conn_handle, svc->start_handle, svc->end_handle,
                                         &chr_uuid.u, cts_on_chr_disc, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_gattc_disc_chrs_by_uuid failed rc=%d", rc);
        s_cts.in_progress = false;
    }
    return 0;
}

static int cts_on_chr_disc(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr, void *arg)
{
    (void)arg;
    if (conn_handle != s_cts.conn_handle) {
        return 0;
    }
    if (error->status == BLE_HS_EDONE) {
        if (chr == NULL) {
            ESP_LOGI(TAG, "CTS Current Time characteristic not found");
            s_cts.in_progress = false;
        }
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGW(TAG, "CTS chr disc error=%d", error->status);
        s_cts.in_progress = false;
        return 0;
    }
    if (chr == NULL) {
        return 0;
    }

    int rc = ble_gattc_read(conn_handle, chr->val_handle, cts_on_read, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_gattc_read failed rc=%d", rc);
        s_cts.in_progress = false;
    }
    return 0;
}

static int cts_on_read(uint16_t conn_handle, const struct ble_gatt_error *error,
                       struct ble_gatt_attr *attr, void *arg)
{
    (void)arg;
    if (conn_handle != s_cts.conn_handle) {
        return 0;
    }
    s_cts.in_progress = false;

    if (error->status != 0 || attr == NULL || attr->om == NULL) {
        ESP_LOGW(TAG, "CTS read failed error=%d", error->status);
        return 0;
    }
    uint16_t om_len = OS_MBUF_PKTLEN(attr->om);
    if (om_len < 10) {
        ESP_LOGW(TAG, "CTS payload too short: %u bytes", om_len);
        return 0;
    }

    uint8_t buf[10];
    if (os_mbuf_copydata(attr->om, 0, sizeof(buf), buf) != 0) {
        ESP_LOGW(TAG, "CTS payload copy failed");
        return 0;
    }

    uint16_t year = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    struct tm t = {
        .tm_year = (int)year - 1900,
        .tm_mon = (int)buf[2] - 1,
        .tm_mday = (int)buf[3],
        .tm_hour = (int)buf[4],
        .tm_min = (int)buf[5],
        .tm_sec = (int)buf[6],
        .tm_isdst = -1,
    };
    if (year < 2024U || year > 2100U || t.tm_mon < 0 || t.tm_mon > 11 ||
        t.tm_mday < 1 || t.tm_mday > 31) {
        ESP_LOGW(TAG, "CTS time out of range y=%u m=%d d=%d", year, t.tm_mon + 1, t.tm_mday);
        return 0;
    }

    time_t epoch = mktime(&t);
    if (epoch == (time_t)-1) {
        ESP_LOGW(TAG, "mktime failed");
        return 0;
    }

    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    if (settimeofday(&tv, NULL) != 0) {
        ESP_LOGW(TAG, "settimeofday failed");
        return 0;
    }

    s_cts.synced = true;
    ESP_LOGI(TAG, "CTS time set: %04u-%02u-%02u %02u:%02u:%02u",
             year, buf[2], buf[3], buf[4], buf[5], buf[6]);

    if (s_cts.cfg.time_cb != NULL) {
        s_cts.cfg.time_cb(&tv, s_cts.cfg.user_ctx);
    }
    return 0;
}
