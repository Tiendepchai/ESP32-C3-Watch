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
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "notification_store.h"
#include "storage_manager.h"

static const char *TAG = BOARD_TAG_APP;

typedef enum {
    APP_EVENT_BLE_STATE = 0,
    APP_EVENT_BOND,
    APP_EVENT_NOTIFICATION,
    APP_EVENT_CONFIG_CHANGED,
    APP_EVENT_BUTTON,
    APP_EVENT_FILTER_OVERLAY_TIMEOUT,
    APP_EVENT_REBOOT,
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    bool complete;
    union {
        ble_manager_state_t ble_state;
        bool bonded;
        button_manager_event_t button_event;
        notification_record_t notification;
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
    esp_timer_handle_t filter_overlay_timer;
    esp_timer_handle_t reboot_timer;
    notification_record_t snapshot_records[BOARD_NOTIFICATION_QUEUE_MAX];
    notification_record_t filtered_records[BOARD_NOTIFICATION_QUEUE_MAX];
} app_ctx_t;

static app_ctx_t s_app;

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
    case NOTIFICATION_FILTER_MESSAGES_SOCIAL:
        return "Messages/Social";
    case NOTIFICATION_FILTER_IMPORTANT_ONLY:
        return "Important only";
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

static bool app_text_contains_token(const char *text, const char *token)
{
    size_t text_len;
    size_t token_len;

    if (text == NULL || token == NULL) {
        return false;
    }

    text_len = strlen(text);
    token_len = strlen(token);
    if (token_len == 0U || text_len < token_len) {
        return false;
    }

    for (size_t i = 0; i <= (text_len - token_len); ++i) {
        size_t j = 0;
        while (j < token_len) {
            char a = text[i + j];
            char b = token[j];

            if (a >= 'A' && a <= 'Z') {
                a = (char)(a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z') {
                b = (char)(b - 'A' + 'a');
            }
            if (a != b) {
                break;
            }
            j++;
        }
        if (j == token_len) {
            return true;
        }
    }

    return false;
}

static bool app_is_default_allowed_app_id(const char *app_id)
{
    static const char *const tokens[] = {
        "telegram",
        "messenger",
        "zalo",
        "momo",
        "mservice",
        "tpbank",
        "tpb",
        "mbbank",
        "mb mobile",
        "mbmobile",
        "instagram",
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

static bool app_is_message_social_record(const notification_record_t *record)
{
    static const char *const tokens[] = {
        "telegram",
        "messenger",
        "zalo",
        "instagram",
    };

    if (record == NULL) {
        return false;
    }

    if (record->category_id == ANCS_CATEGORY_ID_SOCIAL) {
        return true;
    }

    for (size_t i = 0; i < sizeof(tokens) / sizeof(tokens[0]); ++i) {
        if (app_text_contains_token(record->app_id, tokens[i])) {
            return true;
        }
    }

    return false;
}

static bool app_record_matches_filter(const notification_record_t *record)
{
    bool calls_allowed = true;
    bool app_allowed = false;

    if (record == NULL || !record->valid) {
        return false;
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

        app_allowed = app_is_default_allowed_app_id(record->app_id);
        if (storage_manager_is_notification_app_allowed(record->app_id, &app_allowed) && !app_allowed) {
            return false;
        }
        if (!app_allowed) {
            return false;
        }
    }

    switch (s_app.filter) {
    case NOTIFICATION_FILTER_ALL:
        return true;
    case NOTIFICATION_FILTER_CALLS:
        return app_is_call_category(record->category_id);
    case NOTIFICATION_FILTER_MESSAGES_SOCIAL:
        return app_is_message_social_record(record);
    case NOTIFICATION_FILTER_IMPORTANT_ONLY:
        return (record->event_flags & ANCS_EVENT_FLAG_IMPORTANT) != 0U;
    default:
        return true;
    }
}

static size_t app_get_filtered_notifications(void)
{
    size_t total;
    size_t filtered = 0;

    memset(s_app.snapshot_records, 0, sizeof(s_app.snapshot_records));
    memset(s_app.filtered_records, 0, sizeof(s_app.filtered_records));

    total = notification_store_get_all(s_app.store, s_app.snapshot_records, BOARD_NOTIFICATION_QUEUE_MAX);
    for (size_t i = 0; i < total && filtered < BOARD_NOTIFICATION_QUEUE_MAX; ++i) {
        if (app_record_matches_filter(&s_app.snapshot_records[i])) {
            s_app.filtered_records[filtered++] = s_app.snapshot_records[i];
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

    app_or_category = display_record.app_id[0] != '\0'
                      ? display_record.app_id
                      : ancs_category_id_to_string(display_record.category_id);
    display_show_notification(s_app.display, app_or_category, display_record.title, display_record.message);
}

static bool app_record_needs_details(const notification_record_t *record)
{
    return record != NULL &&
           record->valid &&
           !record->details_complete &&
           record->event_id != ANCS_EVENT_ID_NOTIFICATION_REMOVED;
}

static void app_request_record_details(size_t index)
{
    esp_err_t rc;

    if (index >= s_app.filtered_count || !app_record_needs_details(&s_app.filtered_records[index])) {
        return;
    }

    rc = ble_manager_request_notification_details(&s_app.filtered_records[index], true);
    if (rc != ESP_OK && rc != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "failed to queue detail fetch for uid=%" PRIu32 ", rc=%s",
                 s_app.filtered_records[index].uid, esp_err_to_name(rc));
    }
}

static void app_request_selected_window_details(void)
{
    if (s_app.filtered_count == 0U || s_app.selected_index >= s_app.filtered_count) {
        return;
    }

    if ((s_app.selected_index + 1U) < s_app.filtered_count) {
        app_request_record_details(s_app.selected_index + 1U);
    }
    if (s_app.selected_index > 0U) {
        app_request_record_details(s_app.selected_index - 1U);
    }
    app_request_record_details(s_app.selected_index);
}

static bool app_refresh_notification_view(void)
{
    size_t count = app_get_filtered_notifications();

    if (count == 0U) {
        s_app.selected_index = 0;
        app_set_state(APP_STATE_ANCS_READY);
        display_show_status(s_app.display, app_status_text_for_state(s_app.state));
        return false;
    }

    if (s_app.selected_index >= count) {
        s_app.selected_index = 0;
    }

    app_set_state(APP_STATE_SHOWING_NOTIFICATION);
    app_render_notification(&s_app.filtered_records[s_app.selected_index]);
    app_request_selected_window_details();
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
        display_show_status(s_app.display, app_status_text_for_ble_state(ble_state));
        break;
    case BLE_MANAGER_STATE_ANCS_READY:
        app_set_state(APP_STATE_ANCS_READY);
        if (!app_refresh_notification_view()) {
            display_show_status(s_app.display, app_status_text_for_ble_state(ble_state));
        }
        break;
    case BLE_MANAGER_STATE_DISCONNECTED:
        s_app.selected_index = 0;
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

    if (record->event_id == ANCS_EVENT_ID_NOTIFICATION_REMOVED) {
        app_cancel_filter_overlay();
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
            merged.title[0] = '\0';
            merged.message[0] = '\0';
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

    notification_store_upsert(s_app.store, &merged);
    if (complete && merged.app_id[0] != '\0') {
        if (storage_manager_track_notification_app(merged.app_id,
                                                   app_is_default_allowed_app_id(merged.app_id),
                                                   &app_added) != ESP_OK) {
            ESP_LOGW(TAG, "failed to track app_id=%s", merged.app_id);
        } else if (app_added) {
            ble_manager_notify_config_changed();
        }
    }
    pre_existing = (merged.event_flags & ANCS_EVENT_FLAG_PRE_EXISTING) != 0U;
    selected_matches = s_app.filtered_count > 0U &&
                       s_app.selected_index < s_app.filtered_count &&
                       s_app.filtered_records[s_app.selected_index].uid == merged.uid;

    if (!complete) {
        if (app_record_matches_filter(&merged) && s_app.state == APP_STATE_ANCS_READY) {
            app_refresh_notification_view();
        } else if (selected_matches) {
            app_refresh_notification_view();
        } else {
            app_request_selected_window_details();
        }
        return;
    }

    if (app_record_matches_filter(&merged)) {
        if (pre_existing) {
            if (selected_matches || s_app.state == APP_STATE_ANCS_READY) {
                app_refresh_notification_view();
            } else {
                app_request_selected_window_details();
            }
            return;
        }

        app_cancel_filter_overlay();
        s_app.selected_index = 0;
        app_refresh_notification_view();
        return;
    }

    app_request_selected_window_details();
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
    app_set_state(APP_STATE_SHOWING_NOTIFICATION);
    app_render_notification(&s_app.filtered_records[s_app.selected_index]);
    app_request_selected_window_details();
}

static void app_clear_current_notification(void)
{
    size_t count = app_get_filtered_notifications();

    app_cancel_filter_overlay();

    if (count == 0U) {
        display_show_status(s_app.display, app_status_text_for_state(s_app.state));
        return;
    }

    if (s_app.selected_index >= count) {
        s_app.selected_index = 0;
    }

    ESP_LOGI(TAG, "clearing local notification uid=%" PRIu32, s_app.filtered_records[s_app.selected_index].uid);
    notification_store_remove_by_uid(s_app.store, s_app.filtered_records[s_app.selected_index].uid);
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
    app_refresh_notification_view();
}

static void app_worker_task(void *arg)
{
    app_event_t event;
    (void)arg;

    while (xQueueReceive(s_app.queue, &event, portMAX_DELAY) == pdTRUE) {
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
            break;
        case APP_EVENT_CONFIG_CHANGED:
            app_handle_config_changed();
            break;
        case APP_EVENT_BUTTON:
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

    ESP_ERROR_CHECK(esp_timer_create(&filter_overlay_timer_args, &s_app.filter_overlay_timer));
    ESP_ERROR_CHECK(esp_timer_create(&reboot_timer_args, &s_app.reboot_timer));

    BaseType_t task_ok = xTaskCreate(app_worker_task, "app_worker", 6144, NULL, 5, &s_app.task);
    ESP_ERROR_CHECK(task_ok == pdPASS ? ESP_OK : ESP_FAIL);

    button_cfg.event_cb = app_button_cb;
    button_cfg.user_ctx = s_app.queue;
    ESP_ERROR_CHECK(button_manager_init(&button_cfg));

    ble_cfg.device_name = BOARD_DEVICE_NAME;
    ble_cfg.state_cb = app_ble_state_cb;
    ble_cfg.bond_cb = app_bond_cb;
    ble_cfg.notification_cb = app_notification_cb;
    ble_cfg.config_changed_cb = app_config_changed_cb;
    ble_cfg.user_ctx = s_app.queue;
    ESP_ERROR_CHECK(ble_manager_init(&ble_cfg));
}
