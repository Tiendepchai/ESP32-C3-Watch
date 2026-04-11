#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "notification_store.h"
#include "os/os_mbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*ready_cb)(bool attr_channel_ready, void *user_ctx);
    void (*notification_cb)(const notification_record_t *record, bool complete, void *user_ctx);
    void *user_ctx;
} ancs_client_config_t;

esp_err_t ancs_client_init(const ancs_client_config_t *config);
void ancs_client_reset(void);
esp_err_t ancs_client_start_discovery(uint16_t conn_handle);
esp_err_t ancs_client_schedule_discovery(uint16_t conn_handle, uint32_t delay_ms);
void ancs_client_set_mtu(uint16_t mtu);
bool ancs_client_is_ready(void);
bool ancs_client_is_attr_channel_ready(void);
esp_err_t ancs_client_request_notification_details(const notification_record_t *record, bool prioritize);
int ancs_client_handle_notify_rx(uint16_t conn_handle, uint16_t attr_handle, const struct os_mbuf *om);

#ifdef __cplusplus
}
#endif
