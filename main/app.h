#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_STATE_BOOT = 0,
    APP_STATE_WAITING_FOR_PHONE,
    APP_STATE_SCANNING,
    APP_STATE_CONNECTING,
    APP_STATE_CONNECTED,
    APP_STATE_ANCS_READY,
    APP_STATE_SHOWING_NOTIFICATION,
    APP_STATE_SHOWING_NAVIGATION,
    APP_STATE_WATCHFACE,
    APP_STATE_DISCONNECTED,
    APP_STATE_RECONNECTING,
} app_state_t;

void app_start(void);

#ifdef __cplusplus
}
#endif
