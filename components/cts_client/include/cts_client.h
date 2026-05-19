#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*cts_time_cb_t)(const struct timeval *tv, void *user_ctx);

typedef struct {
    cts_time_cb_t time_cb;
    void *user_ctx;
} cts_client_config_t;

esp_err_t cts_client_init(const cts_client_config_t *config);
esp_err_t cts_client_start_discovery(uint16_t conn_handle);
void cts_client_reset(void);
bool cts_client_is_time_synced(void);

#ifdef __cplusplus
}
#endif
