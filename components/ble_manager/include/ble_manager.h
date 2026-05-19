#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "notification_store.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BLE_MANAGER_STATE_IDLE = 0,
    BLE_MANAGER_STATE_STACK_READY,
    BLE_MANAGER_STATE_ADVERTISING,
    BLE_MANAGER_STATE_CONNECTING,
    BLE_MANAGER_STATE_CONNECTED,
    BLE_MANAGER_STATE_SECURED,
    BLE_MANAGER_STATE_ANCS_READY,
    BLE_MANAGER_STATE_DISCONNECTED,
    BLE_MANAGER_STATE_RECONNECTING,
} ble_manager_state_t;

typedef struct {
    bool active;
    uint32_t sequence;
    char source[BOARD_NAV_SOURCE_MAX_LEN];
    char title[BOARD_ANCS_TITLE_MAX_LEN];
    char instruction[BOARD_ANCS_MESSAGE_MAX_LEN];
    char distance[BOARD_NAV_DISTANCE_MAX_LEN];
    char eta[BOARD_NAV_ETA_MAX_LEN];
} ble_manager_navigation_state_t;

typedef struct {
    const char *device_name;
    void (*state_cb)(ble_manager_state_t state, void *user_ctx);
    void (*bond_cb)(bool bonded, void *user_ctx);
    void (*notification_cb)(const notification_record_t *record, bool complete, void *user_ctx);
    void (*config_changed_cb)(void *user_ctx);
    void (*navigation_cb)(const ble_manager_navigation_state_t *state, void *user_ctx);
    void *user_ctx;
} ble_manager_config_t;

esp_err_t ble_manager_init(const ble_manager_config_t *config);
bool ble_manager_has_bonded_peer(void);
esp_err_t ble_manager_clear_bonds(void);
esp_err_t ble_manager_request_notification_details(const notification_record_t *record, bool prioritize);
void ble_manager_notify_config_changed(void);
const char *ble_manager_state_to_string(ble_manager_state_t state);

#ifdef __cplusplus
}
#endif
