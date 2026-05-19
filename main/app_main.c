#include "app.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "ancs_parser.h"
#include "ble_manager.h"
#include "board_config.h"
#include "button_manager.h"
#include "display_manager.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <time.h>
#include "cts_client.h"
#include "notification_store.h"
#include "power_manager.h"
#include "storage_manager.h"

/* Đặt 1 khi đã wire xong battery divider + TP4056 STAT. */
#define APP_ENABLE_POWER_MONITOR 0

static const char *TAG = BOARD_TAG_APP;

#define APP_DETAIL_REQUEST_COOLDOWN_MS 5000ULL

typedef enum {
    APP_EVENT_BLE_STATE = 0,
    APP_EVENT_BOND,
    APP_EVENT_NOTIFICATION,
    APP_EVENT_CONFIG_CHANGED,
    APP_EVENT_NAVIGATION,
    APP_EVENT_BUTTON,
    APP_EVENT_FILTER_OVERLAY_TIMEOUT,
    APP_EVENT_REBOOT,
    APP_EVENT_POWER,
    APP_EVENT_DISPLAY_AUTO_OFF,
    APP_EVENT_WATCHFACE_TICK,
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    bool complete;
    union {
        ble_manager_state_t ble_state;
        bool bonded;
        button_manager_event_t button_event;
        ble_manager_navigation_state_t navigation;
        notification_record_t notification;
        power_state_t power;
    } data;
} app_event_t;

typedef struct {
    QueueHandle_t queue;
    TaskHandle_t task;
    display_handle_t display;
    notification_store_handle_t store;
    app_state_t state;
    notification_filter_t filter;
    size_t selected_index;
    size_t filtered_count;
    bool bonded;
    bool filter_overlay_visible;
    bool reboot_pending;
    bool display_active;
    esp_timer_handle_t filter_overlay_timer;
    esp_timer_handle_t reboot_timer;
    esp_timer_handle_t display_off_timer;
    esp_timer_handle_t watchface_timer;
    ble_manager_navigation_state_t navigation;
    power_state_t power;
    uint32_t detail_request_uids[BOARD_NOTIFICATION_QUEUE_MAX];
    uint64_t detail_request_ms[BOARD_NOTIFICATION_QUEUE_MAX];
    uint32_t filtered_uids[BOARD_NOTIFICATION_QUEUE_MAX];
} app_ctx_t;

static app_ctx_t s_app;

static void app_set_state(app_state_t state);
static void app_apply_display_off_policy(void);

static uint64_t app_now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

static void app_reset_detail_requests(void)
{
    memset(s_app.detail_request_uids, 0, sizeof(s_app.detail_request_uids));
    memset(s_app.detail_request_ms, 0, sizeof(s_app.detail_request_ms));
}

static void app_forget_detail_request(uint32_t uid)
{
    if (uid == 0U) {
        return;
    }

    for (size_t i = 0; i < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        if (s_app.detail_request_uids[i] == uid) {
            s_app.detail_request_uids[i] = 0;
            s_app.detail_request_ms[i] = 0;
            return;
        }
    }
}

static bool app_recently_requested_details(uint32_t uid)
{
    uint64_t now_ms;

    if (uid == 0U) {
        return true;
    }

    now_ms = app_now_ms();
    for (size_t i = 0; i < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        if (s_app.detail_request_uids[i] != uid) {
            continue;
        }
        if ((now_ms - s_app.detail_request_ms[i]) < APP_DETAIL_REQUEST_COOLDOWN_MS) {
            return true;
        }
        s_app.detail_request_uids[i] = 0;
        s_app.detail_request_ms[i] = 0;
        return false;
    }

    return false;
}

static void app_note_detail_request(uint32_t uid)
{
    size_t target = BOARD_NOTIFICATION_QUEUE_MAX;
    uint64_t oldest_ms = UINT64_MAX;
    uint64_t now_ms;

    if (uid == 0U) {
        return;
    }

    now_ms = app_now_ms();
    for (size_t i = 0; i < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        if (s_app.detail_request_uids[i] == uid) {
            target = i;
            break;
        }
        if (s_app.detail_request_uids[i] == 0U) {
            target = i;
            break;
        }
        if (s_app.detail_request_ms[i] <= oldest_ms) {
            oldest_ms = s_app.detail_request_ms[i];
            target = i;
        }
    }

    if (target < BOARD_NOTIFICATION_QUEUE_MAX) {
        s_app.detail_request_uids[target] = uid;
        s_app.detail_request_ms[target] = now_ms;
    }
}

static const char *app_state_to_string(app_state_t state)
{
    switch (state) {
    case APP_STATE_BOOT:
        return "BOOT";
    case APP_STATE_WAITING_FOR_PHONE:
        return "WAITING_FOR_PHONE";
    case APP_STATE_SCANNING:
        return "SCANNING";
    case APP_STATE_CONNECTING:
        return "CONNECTING";
    case APP_STATE_CONNECTED:
        return "CONNECTED";
    case APP_STATE_ANCS_READY:
        return "ANCS_READY";
    case APP_STATE_SHOWING_NOTIFICATION:
        return "SHOWING_NOTIFICATION";
    case APP_STATE_SHOWING_NAVIGATION:
        return "SHOWING_NAVIGATION";
    case APP_STATE_WATCHFACE:
        return "WATCHFACE";
    case APP_STATE_DISCONNECTED:
        return "DISCONNECTED";
    case APP_STATE_RECONNECTING:
        return "RECONNECTING";
    default:
        return "UNKNOWN";
    }
}

static const char *app_filter_to_string(notification_filter_t filter)
{
    switch (filter) {
    case NOTIFICATION_FILTER_ALL:
        return "All";
    case NOTIFICATION_FILTER_CALLS:
        return "Calls";
    case NOTIFICATION_FILTER_NAVIGATION:
        return "Navigation";
    default:
        return "All";
    }
}

static bool app_is_call_category(uint8_t category_id)
{
    return category_id == ANCS_CATEGORY_ID_INCOMING_CALL ||
           category_id == ANCS_CATEGORY_ID_MISSED_CALL ||
           category_id == ANCS_CATEGORY_ID_VOICEMAIL;
}

/* Case-insensitive substring match. */
static bool app_text_contains_token(const char *text, const char *token)
{
    if (text == NULL || token == NULL) {
        return false;
    }
    size_t text_len = strlen(text);
    size_t token_len = strlen(token);
    if (token_len == 0U || text_len < token_len) {
        return false;
    }
    for (size_t i = 0; i <= (text_len - token_len); ++i) {
        size_t j = 0;
        while (j < token_len) {
            char a = text[i + j];
            char b = token[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
            j++;
        }
        if (j == token_len) {
            return true;
        }
    }
    return false;
}

/* Whitelist các app navigation đã verify. Match qua app_id từ ANCS (substring,
 * case-insensitive) để tránh false-positive như Ebooking đặt category=LOCATION. */
static bool app_is_navigation_app_id(const char *app_id)
{
    static const char *const tokens[] = {
        "google.maps",   /* com.google.Maps */
        "apple.maps",    /* com.apple.Maps  */
        "waze",          /* com.waze.iphone */
    };
    if (app_id == NULL || app_id[0] == '\0') {
        return false;
    }
    for (size_t i = 0; i < sizeof(tokens) / sizeof(tokens[0]); ++i) {
        if (app_text_contains_token(app_id, tokens[i])) {
            return true;
        }
    }
    return false;
}

static bool app_is_navigation_record(const notification_record_t *record)
{
    if (record == NULL) {
        return false;
    }
    /* Chỉ tin app_id từ whitelist. Bỏ qua category_id vì iOS lump tất cả
     * mọi notif liên quan vị trí (booking, ride-share, ...) vào LOCATION. */
    return app_is_navigation_app_id(record->app_id);
}

static void app_prepare_navigation_display(notification_record_t *record)
{
    char primary[BOARD_ANCS_TITLE_MAX_LEN] = {0};
    char secondary[BOARD_ANCS_MESSAGE_MAX_LEN] = {0};

    if (record == NULL) {
        return;
    }

    strlcpy(primary, record->title, sizeof(primary));
    strlcpy(secondary, record->message, sizeof(secondary));

    if (primary[0] == '\0' && secondary[0] != '\0') {
        strlcpy(primary, secondary, sizeof(primary));
        secondary[0] = '\0';
    }

    strlcpy(record->title, primary, sizeof(record->title));
    strlcpy(record->message, secondary, sizeof(record->message));
}

/* Detect turn direction từ text (case-insensitive substring).
 * Hỗ trợ tiếng Anh (Maps EN locale) và tiếng Việt không dấu.
 * U-turn check trước "turn" để không nuốt nhầm. */
static nav_icon_t app_detect_nav_direction(const char *primary, const char *secondary)
{
    const char *texts[2] = { primary, secondary };
    for (int i = 0; i < 2; ++i) {
        const char *t = texts[i];
        if (t == NULL || t[0] == '\0') continue;

        if (app_text_contains_token(t, "u-turn") ||
            app_text_contains_token(t, "u turn") ||
            app_text_contains_token(t, "uturn") ||
            app_text_contains_token(t, "quay dau") ||
            app_text_contains_token(t, "quay \xc4\x91\xe1\xba\xa7u")) { /* "quay đầu" */
            return NAV_ICON_UTURN;
        }
        if (app_text_contains_token(t, "left") ||
            app_text_contains_token(t, "trai") ||
            app_text_contains_token(t, "tr\xc3\xa1i")) { /* "trái" */
            return NAV_ICON_LEFT;
        }
        if (app_text_contains_token(t, "right") ||
            app_text_contains_token(t, "phai") ||
            app_text_contains_token(t, "ph\xe1\xba\xa3i")) { /* "phải" */
            return NAV_ICON_RIGHT;
        }
        if (app_text_contains_token(t, "straight") ||
            app_text_contains_token(t, "continue") ||
            app_text_contains_token(t, "thang") ||
            app_text_contains_token(t, "th\xe1\xba\xb3ng") || /* "thẳng" */
            app_text_contains_token(t, "ti\xe1\xba\xbfp t\xe1\xbb\xa5" "c")) { /* "tiếp tục" */
            return NAV_ICON_STRAIGHT;
        }
    }
    return NAV_ICON_NONE;
}

static bool app_record_matches_filter(const notification_record_t *record)
{
    bool calls_allowed = true;

    if (record == NULL || !record->valid) {
        return false;
    }

    if (s_app.filter == NOTIFICATION_FILTER_ALL) {
        return true;
    }

    if (app_is_call_category(record->category_id)) {
        if (storage_manager_get_calls_allowed(&calls_allowed) != ESP_OK) {
            calls_allowed = true;
        }
        if (!calls_allowed) {
            return false;
        }
    } else {
        if (!record->details_complete && record->app_id[0] == '\0') {
            return s_app.filter == NOTIFICATION_FILTER_ALL;
        }

        if (record->app_id[0] == '\0') {
            return false;
        }
    }

    switch (s_app.filter) {
    case NOTIFICATION_FILTER_ALL:
        return true;
    case NOTIFICATION_FILTER_CALLS:
        return app_is_call_category(record->category_id);
    case NOTIFICATION_FILTER_NAVIGATION:
        return app_is_navigation_record(record);
    default:
        return true;
    }
}

static size_t app_get_filtered_notifications(void)
{
    uint32_t snapshot_uids[BOARD_NOTIFICATION_QUEUE_MAX];
    notification_record_t record;
    size_t total;
    size_t filtered = 0;

    total = notification_store_snapshot_uids(s_app.store, snapshot_uids, BOARD_NOTIFICATION_QUEUE_MAX);
    for (size_t i = 0; i < total && filtered < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        if (notification_store_find_by_uid(s_app.store, snapshot_uids[i], &record) &&
            app_record_matches_filter(&record)) {
            s_app.filtered_uids[filtered++] = snapshot_uids[i];
        }
    }

    s_app.filtered_count = filtered;
    return filtered;
}

static const char *app_status_text_for_ble_state(ble_manager_state_t state)
{
    switch (state) {
    case BLE_MANAGER_STATE_STACK_READY:
        return "Waiting for iPhone";
    case BLE_MANAGER_STATE_ADVERTISING:
        return "Waiting for iPhone";
    case BLE_MANAGER_STATE_CONNECTING:
        return "Connecting...";
    case BLE_MANAGER_STATE_CONNECTED:
    case BLE_MANAGER_STATE_SECURED:
        return "Connected";
    case BLE_MANAGER_STATE_ANCS_READY:
        return "Connected";
    case BLE_MANAGER_STATE_DISCONNECTED:
        return "Disconnected";
    case BLE_MANAGER_STATE_RECONNECTING:
        return "Reconnecting...";
    default:
        return "Booting...";
    }
}

static const char *app_status_text_for_state(app_state_t state)
{
    switch (state) {
    case APP_STATE_WAITING_FOR_PHONE:
    case APP_STATE_SCANNING:
        return "Waiting for iPhone";
    case APP_STATE_CONNECTING:
        return "Connecting...";
    case APP_STATE_CONNECTED:
    case APP_STATE_ANCS_READY:
    case APP_STATE_SHOWING_NOTIFICATION:
    case APP_STATE_SHOWING_NAVIGATION:
    case APP_STATE_WATCHFACE:
        return "Connected";
    case APP_STATE_DISCONNECTED:
        return "Disconnected";
    case APP_STATE_RECONNECTING:
        return "Reconnecting...";
    default:
        return "Booting...";
    }
}

static void app_set_state(app_state_t state)
{
    if (s_app.state == state) {
        return;
    }
    s_app.state = state;
    ESP_LOGI(TAG, "app_state -> %s", app_state_to_string(state));
    app_apply_display_off_policy();
}

static bool app_post_event_with_wait(QueueHandle_t queue, const app_event_t *event, TickType_t wait_ticks)
{
    if (queue == NULL || event == NULL) {
        return false;
    }

    return xQueueSend(queue, event, wait_ticks) == pdTRUE;
}

static bool app_post_event(QueueHandle_t queue, const app_event_t *event)
{
    return app_post_event_with_wait(queue, event, pdMS_TO_TICKS(BOARD_APP_EVENT_SEND_WAIT_MS));
}

static void app_cancel_filter_overlay(void)
{
    s_app.filter_overlay_visible = false;
    if (s_app.filter_overlay_timer != NULL) {
        esp_timer_stop(s_app.filter_overlay_timer);
    }
}

static void app_filter_overlay_timer_cb(void *arg)
{
    QueueHandle_t queue = (QueueHandle_t)arg;
    app_event_t event = {
        .type = APP_EVENT_FILTER_OVERLAY_TIMEOUT,
    };

    if (!app_post_event_with_wait(queue, &event, 0)) {
        ESP_LOGW(TAG, "dropping filter overlay timeout event, app queue full");
    }
}

static void app_reboot_timer_cb(void *arg)
{
    QueueHandle_t queue = (QueueHandle_t)arg;
    app_event_t event = {
        .type = APP_EVENT_REBOOT,
    };

    if (!app_post_event_with_wait(queue, &event, 0)) {
        ESP_LOGW(TAG, "dropping reboot event, app queue full");
    }
}

static void app_display_off_timer_cb(void *arg)
{
    QueueHandle_t queue = (QueueHandle_t)arg;
    app_event_t event = {
        .type = APP_EVENT_DISPLAY_AUTO_OFF,
    };

    if (!app_post_event_with_wait(queue, &event, 0)) {
        ESP_LOGW(TAG, "dropping display auto-off event, app queue full");
    }
}

static void app_watchface_timer_cb(void *arg)
{
    QueueHandle_t queue = (QueueHandle_t)arg;
    app_event_t event = {
        .type = APP_EVENT_WATCHFACE_TICK,
    };

    if (!app_post_event_with_wait(queue, &event, 0)) {
        ESP_LOGW(TAG, "dropping watchface tick, app queue full");
    }
}

static void app_render_watchface(void)
{
    char time_str[8] = "--:--";
    char date_str[20] = "";
    char battery_str[12] = "";
    const char *source_str = "";
    bool synced = cts_client_is_time_synced();

    if (synced) {
        time_t now = time(NULL);
        struct tm tm_local;
        localtime_r(&now, &tm_local);
        snprintf(time_str, sizeof(time_str), "%02d:%02d", tm_local.tm_hour, tm_local.tm_min);
        strftime(date_str, sizeof(date_str), "%a %d %b", &tm_local);
    }

    if (s_app.power.source != POWER_SOURCE_UNKNOWN) {
        snprintf(battery_str, sizeof(battery_str), "%u%%", (unsigned)s_app.power.battery_pct);
        switch (s_app.power.source) {
        case POWER_SOURCE_CHARGING: source_str = "CHRG"; break;
        case POWER_SOURCE_CHARGED:  source_str = "FULL"; break;
        default:                    source_str = "";     break;
        }
    }

    app_set_state(APP_STATE_WATCHFACE);
    display_show_watchface(s_app.display, time_str, date_str, battery_str, source_str);
}

static void app_start_watchface_timer(void)
{
    if (s_app.watchface_timer == NULL) {
        return;
    }
    esp_timer_stop(s_app.watchface_timer);
    /* Tick every 30s — adequate for HH:MM display and battery%. */
    esp_timer_start_periodic(s_app.watchface_timer, 30ULL * 1000ULL * 1000ULL);
}

static void app_stop_watchface_timer(void)
{
    if (s_app.watchface_timer != NULL) {
        esp_timer_stop(s_app.watchface_timer);
    }
}

#if APP_ENABLE_POWER_MONITOR
static void app_power_state_cb(const power_state_t *state, void *user_ctx)
{
    QueueHandle_t queue = (QueueHandle_t)user_ctx;
    app_event_t event = {
        .type = APP_EVENT_POWER,
    };

    if (state != NULL) {
        event.data.power = *state;
    }
    if (!app_post_event(queue, &event)) {
        ESP_LOGW(TAG, "dropping power state event, app queue full");
    }
}
#endif

static void app_apply_display_off_policy(void)
{
    if (s_app.display_off_timer == NULL) {
        return;
    }
    esp_timer_stop(s_app.display_off_timer);
    /* Navigation thường yêu cầu màn hình luôn sáng — bỏ qua auto-off. */
    if (s_app.state == APP_STATE_SHOWING_NAVIGATION) {
        return;
    }
    esp_timer_start_once(s_app.display_off_timer,
                         (uint64_t)BOARD_DISPLAY_AUTO_OFF_MS * 1000ULL);
}

static void app_kick_display_activity(void)
{
    if (s_app.display == NULL) {
        return;
    }
    if (!s_app.display_active) {
        if (display_set_active(s_app.display, true) == ESP_OK) {
            s_app.display_active = true;
            ESP_LOGI(TAG, "display woken by activity");
        }
    }
    app_apply_display_off_policy();
}

static void app_handle_power_state(const power_state_t *state)
{
    if (state == NULL) {
        return;
    }
    s_app.power = *state;
    if (state->critical_battery) {
        ESP_LOGW(TAG, "battery critical (%u%%), consider shutdown", (unsigned)state->battery_pct);
    } else if (state->low_battery) {
        ESP_LOGW(TAG, "battery low (%u%%)", (unsigned)state->battery_pct);
    }
}

static void app_handle_display_auto_off(void)
{
    if (s_app.display == NULL || !s_app.display_active) {
        return;
    }
    if (display_set_active(s_app.display, false) == ESP_OK) {
        s_app.display_active = false;
        ESP_LOGI(TAG, "display auto-off after %u ms idle", (unsigned)BOARD_DISPLAY_AUTO_OFF_MS);
    }
}

static void app_ble_state_cb(ble_manager_state_t state, void *user_ctx)
{
    app_event_t event = {
        .type = APP_EVENT_BLE_STATE,
        .data.ble_state = state,
    };
    QueueHandle_t queue = (QueueHandle_t)user_ctx;
    if (!app_post_event(queue, &event)) {
        ESP_LOGW(TAG, "dropping BLE state event, app queue full");
    }
}

static void app_bond_cb(bool bonded, void *user_ctx)
{
    app_event_t event = {
        .type = APP_EVENT_BOND,
        .data.bonded = bonded,
    };
    QueueHandle_t queue = (QueueHandle_t)user_ctx;
    if (!app_post_event(queue, &event)) {
        ESP_LOGW(TAG, "dropping bond event, app queue full");
    }
}

static void app_config_changed_cb(void *user_ctx)
{
    app_event_t event = {
        .type = APP_EVENT_CONFIG_CHANGED,
    };
    QueueHandle_t queue = (QueueHandle_t)user_ctx;

    if (!app_post_event(queue, &event)) {
        ESP_LOGW(TAG, "dropping config changed event, app queue full");
    }
}

static void app_navigation_cb(const ble_manager_navigation_state_t *state, void *user_ctx)
{
    app_event_t event = {
        .type = APP_EVENT_NAVIGATION,
    };
    QueueHandle_t queue = (QueueHandle_t)user_ctx;

    if (state == NULL) {
        return;
    }

    event.data.navigation = *state;
    if (!app_post_event(queue, &event)) {
        ESP_LOGW(TAG, "dropping navigation event, app queue full");
    }
}

static void app_notification_cb(const notification_record_t *record, bool complete, void *user_ctx)
{
    app_event_t event = {
        .type = APP_EVENT_NOTIFICATION,
        .complete = complete,
    };
    QueueHandle_t queue = (QueueHandle_t)user_ctx;

    if (record == NULL) {
        return;
    }

    event.data.notification = *record;
    if (!app_post_event(queue, &event)) {
        ESP_LOGW(TAG, "dropping notification event uid=%" PRIu32 ", app queue full", record->uid);
    }
}

static void app_button_cb(button_manager_event_t event, void *user_ctx)
{
    app_event_t app_event = {
        .type = APP_EVENT_BUTTON,
        .data.button_event = event,
    };
    QueueHandle_t queue = (QueueHandle_t)user_ctx;

    if (!app_post_event(queue, &app_event)) {
        ESP_LOGW(TAG, "dropping button event=%d, app queue full", event);
    }
}

static bool app_has_active_navigation(void)
{
    return s_app.navigation.active;
}

static void app_render_navigation_view(void)
{
    nav_icon_t icon = app_detect_nav_direction(s_app.navigation.instruction,
                                               s_app.navigation.title);
    app_set_state(APP_STATE_SHOWING_NAVIGATION);
    display_show_navigation(s_app.display,
                            s_app.navigation.source,
                            s_app.navigation.title,
                            s_app.navigation.instruction,
                            s_app.navigation.distance,
                            s_app.navigation.eta,
                            icon);
}

static void app_render_notification(const notification_record_t *record)
{
    notification_record_t display_record = {0};
    const char *app_or_category;

    if (record == NULL) {
        return;
    }

    display_record = *record;
    if (!display_record.details_complete) {
        if (display_record.title[0] == '\0') {
            strlcpy(display_record.title, "(loading)", sizeof(display_record.title));
        }
        if (display_record.message[0] == '\0') {
            strlcpy(display_record.message, "Fetching details...", sizeof(display_record.message));
        }
    }

    nav_icon_t icon = NAV_ICON_NONE;
    if (app_is_navigation_record(&display_record)) {
        app_prepare_navigation_display(&display_record);
        icon = app_detect_nav_direction(display_record.title, display_record.message);
    }

    app_or_category = display_record.app_id[0] != '\0'
                      ? display_record.app_id
                      : ancs_category_id_to_string(display_record.category_id);
    display_show_notification(s_app.display, app_or_category, display_record.title,
                              display_record.message, icon);
}

static bool app_record_needs_details(const notification_record_t *record, bool include_pre_existing)
{
    return record != NULL &&
           record->uid != 0U &&
           record->valid &&
           !record->details_complete &&
           (include_pre_existing || (record->event_flags & ANCS_EVENT_FLAG_PRE_EXISTING) == 0U) &&
           record->event_id != ANCS_EVENT_ID_NOTIFICATION_REMOVED;
}

static bool app_should_surface_pre_existing(const notification_record_t *record, bool selected_matches)
{
    if (!app_record_matches_filter(record)) {
        return false;
    }

    return selected_matches ||
           s_app.filtered_count == 0U ||
           s_app.state == APP_STATE_ANCS_READY ||
           s_app.state == APP_STATE_WATCHFACE;
}

static void app_request_record_details_by_uid(uint32_t uid, bool include_pre_existing)
{
    notification_record_t record;
    esp_err_t rc;

    if (uid == 0U || !notification_store_find_by_uid(s_app.store, uid, &record)) {
        return;
    }
    if (!app_record_needs_details(&record, include_pre_existing)) {
        return;
    }
    if (app_recently_requested_details(uid)) {
        return;
    }

    rc = ble_manager_request_notification_details(&record, true);
    if (rc == ESP_OK) {
        app_note_detail_request(uid);
        return;
    }
    if (rc != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "failed to queue detail fetch for uid=%" PRIu32 ", rc=%s",
                 uid, esp_err_to_name(rc));
    }
}

static void app_request_selected_window_details(bool include_pre_existing)
{
    if (s_app.filtered_count == 0U || s_app.selected_index >= s_app.filtered_count) {
        return;
    }

    if ((s_app.selected_index + 1U) < s_app.filtered_count) {
        app_request_record_details_by_uid(s_app.filtered_uids[s_app.selected_index + 1U], include_pre_existing);
    }
    if (s_app.selected_index > 0U) {
        app_request_record_details_by_uid(s_app.filtered_uids[s_app.selected_index - 1U], include_pre_existing);
    }
    app_request_record_details_by_uid(s_app.filtered_uids[s_app.selected_index], include_pre_existing);
}

static void app_request_background_details(size_t max_requests)
{
    uint32_t snapshot_uids[BOARD_NOTIFICATION_QUEUE_MAX];
    notification_record_t record;
    size_t total;
    size_t requested = 0;
    esp_err_t rc;

    if (max_requests == 0U) {
        return;
    }

    total = notification_store_snapshot_uids(s_app.store, snapshot_uids, BOARD_NOTIFICATION_QUEUE_MAX);
    for (size_t i = 0; i < total && requested < max_requests; ++i) {
        if (!notification_store_find_by_uid(s_app.store, snapshot_uids[i], &record)) {
            continue;
        }
        if (!app_record_needs_details(&record, false)) {
            continue;
        }
        if ((record.event_flags & ANCS_EVENT_FLAG_PRE_EXISTING) != 0U) {
            continue;
        }
        if (app_recently_requested_details(record.uid)) {
            continue;
        }

        rc = ble_manager_request_notification_details(&record, false);
        if (rc == ESP_OK) {
            app_note_detail_request(record.uid);
            requested++;
            continue;
        }
        if (rc == ESP_ERR_INVALID_STATE) {
            break;
        }

        ESP_LOGW(TAG, "failed to queue background detail fetch for uid=%" PRIu32 ", rc=%s",
                 record.uid, esp_err_to_name(rc));
    }
}

static bool app_refresh_notification_view(void)
{
    size_t count = app_get_filtered_notifications();

    if (app_has_active_navigation()) {
        app_stop_watchface_timer();
        app_render_navigation_view();
        app_request_background_details(BOARD_ANCS_PREEXISTING_ENRICH_MAX);
        return true;
    }

    if (count == 0U) {
        s_app.selected_index = 0;
        app_render_watchface();
        app_start_watchface_timer();
        app_request_background_details(BOARD_ANCS_PREEXISTING_ENRICH_MAX);
        return true;
    }

    app_stop_watchface_timer();

    if (s_app.selected_index >= count) {
        s_app.selected_index = 0;
    }

    {
        notification_record_t record;
        uint32_t selected_uid = s_app.filtered_uids[s_app.selected_index];
        if (notification_store_find_by_uid(s_app.store, selected_uid, &record)) {
            app_set_state(APP_STATE_SHOWING_NOTIFICATION);
            app_render_notification(&record);
        }
        app_request_record_details_by_uid(selected_uid, true);
    }
    app_request_selected_window_details(false);
    app_request_background_details(BOARD_ANCS_PREEXISTING_ENRICH_MAX);
    return true;
}

static void app_show_filter_overlay(void)
{
    char status[40];

    snprintf(status, sizeof(status), "Filter: %s", app_filter_to_string(s_app.filter));
    app_cancel_filter_overlay();
    s_app.filter_overlay_visible = true;
    display_show_status(s_app.display, status);
    if (s_app.filter_overlay_timer != NULL) {
        if (esp_timer_start_once(s_app.filter_overlay_timer, BOARD_APP_FILTER_OVERLAY_MS * 1000ULL) != ESP_OK) {
            ESP_LOGW(TAG, "failed to start filter overlay timer");
            s_app.filter_overlay_visible = false;
            app_refresh_notification_view();
        }
    } else {
        s_app.filter_overlay_visible = false;
        app_refresh_notification_view();
    }
}

static void app_handle_ble_state(ble_manager_state_t ble_state)
{
    if (s_app.reboot_pending) {
        return;
    }

    app_cancel_filter_overlay();

    switch (ble_state) {
    case BLE_MANAGER_STATE_STACK_READY:
        app_set_state(APP_STATE_WAITING_FOR_PHONE);
        display_show_status(s_app.display, app_status_text_for_ble_state(ble_state));
        break;
    case BLE_MANAGER_STATE_ADVERTISING:
        app_set_state(APP_STATE_WAITING_FOR_PHONE);
        display_show_status(s_app.display, app_status_text_for_ble_state(ble_state));
        break;
    case BLE_MANAGER_STATE_CONNECTING:
        app_set_state(APP_STATE_CONNECTING);
        display_show_status(s_app.display, app_status_text_for_ble_state(ble_state));
        break;
    case BLE_MANAGER_STATE_CONNECTED:
    case BLE_MANAGER_STATE_SECURED:
        app_set_state(APP_STATE_CONNECTED);
        if (app_has_active_navigation()) {
            app_render_navigation_view();
        } else {
            display_show_status(s_app.display, app_status_text_for_ble_state(ble_state));
        }
        break;
    case BLE_MANAGER_STATE_ANCS_READY:
        app_set_state(APP_STATE_ANCS_READY);
        if (app_has_active_navigation()) {
            app_render_navigation_view();
        } else if (!app_refresh_notification_view()) {
            display_show_status(s_app.display, app_status_text_for_ble_state(ble_state));
        }
        break;
    case BLE_MANAGER_STATE_DISCONNECTED:
        memset(&s_app.navigation, 0, sizeof(s_app.navigation));
        s_app.selected_index = 0;
        app_reset_detail_requests();
        app_set_state(APP_STATE_DISCONNECTED);
        notification_store_clear(s_app.store);
        display_show_status(s_app.display, app_status_text_for_ble_state(ble_state));
        break;
    case BLE_MANAGER_STATE_RECONNECTING:
        app_set_state(APP_STATE_RECONNECTING);
        display_show_status(s_app.display, app_status_text_for_ble_state(ble_state));
        break;
    default:
        break;
    }
}

static void app_handle_navigation(const ble_manager_navigation_state_t *state)
{
    if (state == NULL) {
        return;
    }

    s_app.navigation = *state;
    app_cancel_filter_overlay();

    if (uxQueueMessagesWaiting(s_app.queue) > 0) {
        return;
    }

    if (app_has_active_navigation()) {
        ESP_LOGI(TAG, "navigation foreground active source=%s seq=%" PRIu32,
                 s_app.navigation.source,
                 s_app.navigation.sequence);
        app_render_navigation_view();
        return;
    }

    ESP_LOGI(TAG, "navigation foreground cleared");
    app_refresh_notification_view();
}

static void app_handle_notification(const notification_record_t *record, bool complete)
{
    notification_record_t merged = {0};
    bool have_existing = false;
    bool app_added = false;
    bool pre_existing = false;
    bool selected_matches = false;

    if (s_app.reboot_pending) {
        return;
    }

    if (record == NULL) {
        return;
    }

    if (record->uid == 0U) {
        ESP_LOGW(TAG, "dropping malformed notification event with uid=0");
        return;
    }

    if (record->event_id == ANCS_EVENT_ID_NOTIFICATION_REMOVED) {
        app_cancel_filter_overlay();
        app_forget_detail_request(record->uid);
        notification_store_remove_by_uid(s_app.store, record->uid);
        app_refresh_notification_view();
        return;
    }

    have_existing = notification_store_find_by_uid(s_app.store, record->uid, &merged);
    if (have_existing) {
        if (!complete) {
            merged.event_id = record->event_id;
            merged.event_flags = record->event_flags;
            merged.category_id = record->category_id;
            merged.category_count = record->category_count;
            merged.details_complete = false;
        }
        if (record->app_id[0] != '\0') {
            strlcpy(merged.app_id, record->app_id, sizeof(merged.app_id));
        }
        if (record->title[0] != '\0') {
            strlcpy(merged.title, record->title, sizeof(merged.title));
        }
        if (record->message[0] != '\0') {
            strlcpy(merged.message, record->message, sizeof(merged.message));
        }
        if (record->timestamp_ms != 0U) {
            merged.timestamp_ms = record->timestamp_ms;
        }
        if (complete) {
            merged.details_complete = record->details_complete;
        }
        merged.valid = true;
    } else {
        merged = *record;
        merged.details_complete = complete && record->details_complete;
        merged.valid = true;
    }

    if (notification_store_upsert(s_app.store, &merged) != ESP_OK) {
        ESP_LOGW(TAG, "failed to store notification uid=%" PRIu32, merged.uid);
        return;
    }
    if (complete) {
        app_forget_detail_request(merged.uid);
    }
    if (complete && merged.app_id[0] != '\0') {
        if (storage_manager_track_notification_app(merged.app_id, true, &app_added) != ESP_OK) {
            ESP_LOGW(TAG, "failed to track app_id=%s", merged.app_id);
        } else if (app_added) {
            ble_manager_notify_config_changed();
        }
    }
    pre_existing = (merged.event_flags & ANCS_EVENT_FLAG_PRE_EXISTING) != 0U;
    selected_matches = s_app.filtered_count > 0U &&
                       s_app.selected_index < s_app.filtered_count &&
                       s_app.filtered_uids[s_app.selected_index] == merged.uid;

    if (uxQueueMessagesWaiting(s_app.queue) > 0) {
        return;
    }

    if (!complete) {
        if (pre_existing) {
            if (app_should_surface_pre_existing(&merged, selected_matches)) {
                app_cancel_filter_overlay();
                app_kick_display_activity();
                app_refresh_notification_view();
            }
            return;
        }

        if (app_record_matches_filter(&merged) && s_app.state == APP_STATE_ANCS_READY) {
            app_kick_display_activity();
            app_refresh_notification_view();
        } else if (selected_matches) {
            app_kick_display_activity();
            app_refresh_notification_view();
        } else {
            app_request_selected_window_details(false);
            app_request_background_details(BOARD_ANCS_PREEXISTING_ENRICH_MAX);
        }
        return;
    }

    if (app_record_matches_filter(&merged)) {
        if (pre_existing) {
            if (selected_matches || s_app.state == APP_STATE_ANCS_READY) {
                app_refresh_notification_view();
            } else {
                app_request_selected_window_details(false);
                app_request_background_details(BOARD_ANCS_PREEXISTING_ENRICH_MAX);
            }
            return;
        }

        app_cancel_filter_overlay();
        s_app.selected_index = 0;
        app_refresh_notification_view();
        return;
    }

    app_request_selected_window_details(false);
    app_request_background_details(BOARD_ANCS_PREEXISTING_ENRICH_MAX);
}

static void app_cycle_filter(void)
{
    s_app.filter = (notification_filter_t)((s_app.filter + 1) % NOTIFICATION_FILTER_COUNT);
    s_app.selected_index = 0;
    storage_manager_set_notification_filter(s_app.filter);
    ESP_LOGI(TAG, "notification filter -> %s", app_filter_to_string(s_app.filter));
    app_show_filter_overlay();
}

static void app_step_notification(int direction)
{
    size_t count = app_get_filtered_notifications();

    app_cancel_filter_overlay();

    if (app_has_active_navigation()) {
        app_render_navigation_view();
        return;
    }

    if (count == 0U) {
        app_refresh_notification_view();
        return;
    }

    if (direction < 0) {
        s_app.selected_index = (s_app.selected_index == 0U) ? (count - 1U) : (s_app.selected_index - 1U);
    } else {
        s_app.selected_index = (s_app.selected_index + 1U) % count;
    }

    ESP_LOGI(TAG, "button nav -> %s, index=%u/%u filter=%s",
             direction < 0 ? "previous" : "next",
             (unsigned)(s_app.selected_index + 1U),
             (unsigned)count,
             app_filter_to_string(s_app.filter));
    {
        notification_record_t record;
        if (notification_store_find_by_uid(s_app.store, s_app.filtered_uids[s_app.selected_index], &record)) {
            app_set_state(APP_STATE_SHOWING_NOTIFICATION);
            app_render_notification(&record);
        }
    }
    app_request_selected_window_details(true);
}

static void app_clear_current_notification(void)
{
    size_t count = app_get_filtered_notifications();

    app_cancel_filter_overlay();

    if (app_has_active_navigation()) {
        app_render_navigation_view();
        return;
    }

    if (count == 0U) {
        display_show_status(s_app.display, app_status_text_for_state(s_app.state));
        return;
    }

    if (s_app.selected_index >= count) {
        s_app.selected_index = 0;
    }

    {
        uint32_t uid = s_app.filtered_uids[s_app.selected_index];
        ESP_LOGI(TAG, "clearing local notification uid=%" PRIu32, uid);
        app_forget_detail_request(uid);
        notification_store_remove_by_uid(s_app.store, uid);
    }
    app_refresh_notification_view();
}

static void app_schedule_clear_bonds_and_reboot(void)
{
    ESP_LOGW(TAG, "button combo -> clear bonds and reboot");
    app_cancel_filter_overlay();
    s_app.reboot_pending = true;
    display_show_status(s_app.display, "Clearing bonds...");
    if (s_app.reboot_timer != NULL) {
        esp_timer_stop(s_app.reboot_timer);
        if (esp_timer_start_once(s_app.reboot_timer, BOARD_APP_REBOOT_DELAY_MS * 1000ULL) != ESP_OK) {
            ESP_LOGW(TAG, "failed to start reboot timer");
            s_app.reboot_pending = false;
        }
    } else {
        s_app.reboot_pending = false;
    }
}

static void app_execute_clear_bonds_and_reboot(void)
{
    if (!s_app.reboot_pending) {
        return;
    }

    s_app.reboot_pending = false;
    ble_manager_clear_bonds();
    storage_manager_set_bonded(false);
    app_reset_detail_requests();
    notification_store_clear(s_app.store);
    esp_restart();
}

static void app_handle_button(button_manager_event_t event)
{
    if (s_app.reboot_pending && event != BUTTON_MANAGER_EVENT_AB_LONG) {
        return;
    }

    switch (event) {
    case BUTTON_MANAGER_EVENT_A_SHORT:
        app_step_notification(-1);
        break;
    case BUTTON_MANAGER_EVENT_B_SHORT:
        app_step_notification(1);
        break;
    case BUTTON_MANAGER_EVENT_A_LONG:
        app_clear_current_notification();
        break;
    case BUTTON_MANAGER_EVENT_B_LONG:
        app_cycle_filter();
        break;
    case BUTTON_MANAGER_EVENT_AB_LONG:
        app_schedule_clear_bonds_and_reboot();
        break;
    default:
        break;
    }
}

static void app_handle_config_changed(void)
{
    if (app_has_active_navigation()) {
        app_render_navigation_view();
    } else {
        app_refresh_notification_view();
    }
}

static bool app_worker_task_wdt_subscribed(void)
{
    return esp_task_wdt_status(NULL) == ESP_OK;
}

static void app_worker_task(void *arg)
{
    app_event_t event;
    const TickType_t wdt_poll_ticks = pdMS_TO_TICKS(1000);
    (void)arg;

    for (;;) {
        BaseType_t received = xQueueReceive(s_app.queue, &event, wdt_poll_ticks);
        bool wdt_subscribed = app_worker_task_wdt_subscribed();

        if (wdt_subscribed) {
            esp_err_t rc = esp_task_wdt_reset();
            if (rc != ESP_OK) {
                ESP_LOGW(TAG, "failed to reset app_worker task watchdog, rc=%s", esp_err_to_name(rc));
            }
        }

        if (received != pdTRUE) {
            continue;
        }

        switch (event.type) {
        case APP_EVENT_BLE_STATE:
            app_handle_ble_state(event.data.ble_state);
            break;
        case APP_EVENT_BOND:
            s_app.bonded = event.data.bonded;
            storage_manager_set_bonded(event.data.bonded);
            break;
        case APP_EVENT_NOTIFICATION:
            app_handle_notification(&event.data.notification, event.complete);
            if (event.complete) {
                app_kick_display_activity();
            }
            break;
        case APP_EVENT_CONFIG_CHANGED:
            app_handle_config_changed();
            break;
        case APP_EVENT_NAVIGATION:
            app_handle_navigation(&event.data.navigation);
            app_kick_display_activity();
            break;
        case APP_EVENT_BUTTON:
            app_kick_display_activity();
            app_handle_button(event.data.button_event);
            break;
        case APP_EVENT_FILTER_OVERLAY_TIMEOUT:
            if (s_app.filter_overlay_visible) {
                s_app.filter_overlay_visible = false;
                app_refresh_notification_view();
            }
            break;
        case APP_EVENT_REBOOT:
            app_execute_clear_bonds_and_reboot();
            break;
        case APP_EVENT_POWER:
            app_handle_power_state(&event.data.power);
            if (s_app.state == APP_STATE_WATCHFACE) {
                app_render_watchface();
            }
            break;
        case APP_EVENT_DISPLAY_AUTO_OFF:
            app_handle_display_auto_off();
            break;
        case APP_EVENT_WATCHFACE_TICK:
            if (s_app.state == APP_STATE_WATCHFACE && s_app.display_active) {
                app_render_watchface();
            }
            break;
        default:
            break;
        }
    }
}

void app_start(void)
{
    ble_manager_config_t ble_cfg = {0};
    button_manager_config_t button_cfg = {0};
    bool bonded = false;
    bool inverted = false;
    notification_filter_t filter = NOTIFICATION_FILTER_ALL;

    memset(&s_app, 0, sizeof(s_app));
    s_app.state = (app_state_t)-1;
    s_app.filter = NOTIFICATION_FILTER_ALL;
    app_set_state(APP_STATE_BOOT);

#if CONFIG_PM_ENABLE
    {
        esp_pm_config_t pm_config = {
            .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
            .min_freq_mhz = 10,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
            .light_sleep_enable = true,
#else
            .light_sleep_enable = false,
#endif
        };
        ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
        ESP_LOGI(TAG, "esp_pm configured: max=%dMHz min=%dMHz light_sleep=%d",
                 pm_config.max_freq_mhz, pm_config.min_freq_mhz, pm_config.light_sleep_enable);
    }
#endif

    ESP_ERROR_CHECK(storage_manager_init());
    ESP_ERROR_CHECK(display_init(&s_app.display));
    display_show_status(s_app.display, "Booting...");

    storage_manager_get_bonded(&bonded);
    storage_manager_get_display_inverted(&inverted);
    storage_manager_get_notification_filter(&filter);
    display_set_inverted(s_app.display, inverted);
    s_app.bonded = bonded;
    s_app.filter = filter;

    ESP_ERROR_CHECK(notification_store_init(&s_app.store));

    s_app.queue = xQueueCreate(BOARD_APP_EVENT_QUEUE_LEN, sizeof(app_event_t));
    ESP_ERROR_CHECK(s_app.queue != NULL ? ESP_OK : ESP_ERR_NO_MEM);

    const esp_timer_create_args_t filter_overlay_timer_args = {
        .callback = app_filter_overlay_timer_cb,
        .arg = s_app.queue,
        .name = "app_overlay",
    };
    const esp_timer_create_args_t reboot_timer_args = {
        .callback = app_reboot_timer_cb,
        .arg = s_app.queue,
        .name = "app_reboot",
    };
    const esp_timer_create_args_t display_off_timer_args = {
        .callback = app_display_off_timer_cb,
        .arg = s_app.queue,
        .name = "app_disp_off",
    };
    const esp_timer_create_args_t watchface_timer_args = {
        .callback = app_watchface_timer_cb,
        .arg = s_app.queue,
        .name = "app_watchface",
    };

    ESP_ERROR_CHECK(esp_timer_create(&filter_overlay_timer_args, &s_app.filter_overlay_timer));
    ESP_ERROR_CHECK(esp_timer_create(&reboot_timer_args, &s_app.reboot_timer));
    ESP_ERROR_CHECK(esp_timer_create(&display_off_timer_args, &s_app.display_off_timer));
    ESP_ERROR_CHECK(esp_timer_create(&watchface_timer_args, &s_app.watchface_timer));

    s_app.display_active = true;
    s_app.power.source = POWER_SOURCE_UNKNOWN;

    BaseType_t task_ok = xTaskCreate(app_worker_task, "app_worker", 6144, NULL, 5, &s_app.task);
    ESP_ERROR_CHECK(task_ok == pdPASS ? ESP_OK : ESP_FAIL);

    button_cfg.event_cb = app_button_cb;
    button_cfg.user_ctx = s_app.queue;
    ESP_ERROR_CHECK(button_manager_init(&button_cfg));

#if APP_ENABLE_POWER_MONITOR
    power_manager_config_t power_cfg = {
        .state_cb = app_power_state_cb,
        .user_ctx = s_app.queue,
    };
    ESP_ERROR_CHECK(power_manager_init(&power_cfg));
#else
    ESP_LOGI(TAG, "power monitor disabled at compile time");
#endif

    ble_cfg.device_name = BOARD_DEVICE_NAME;
    ble_cfg.state_cb = app_ble_state_cb;
    ble_cfg.bond_cb = app_bond_cb;
    ble_cfg.notification_cb = app_notification_cb;
    ble_cfg.config_changed_cb = app_config_changed_cb;
    ble_cfg.navigation_cb = app_navigation_cb;
    ble_cfg.user_ctx = s_app.queue;
    ESP_ERROR_CHECK(ble_manager_init(&ble_cfg));

    app_kick_display_activity();
}
