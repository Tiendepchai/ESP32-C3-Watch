#include "ancs_parser.h"

#include <string.h>
#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = BOARD_TAG_ANCS_PARSER;

static void ancs_safe_copy(char *dst, size_t dst_size, const uint8_t *src, size_t src_len)
{
    size_t copy_len;

    if (dst == NULL || dst_size == 0) {
        return;
    }

    if (src == NULL || src_len == 0) {
        dst[0] = '\0';
        return;
    }

    copy_len = src_len;
    if (copy_len >= dst_size) {
        copy_len = dst_size - 1;
    }
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

const char *ancs_event_id_to_string(uint8_t event_id)
{
    switch (event_id) {
    case ANCS_EVENT_ID_NOTIFICATION_ADDED:
        return "Added";
    case ANCS_EVENT_ID_NOTIFICATION_MODIFIED:
        return "Modified";
    case ANCS_EVENT_ID_NOTIFICATION_REMOVED:
        return "Removed";
    default:
        return "Unknown";
    }
}

const char *ancs_category_id_to_string(uint8_t category_id)
{
    switch (category_id) {
    case ANCS_CATEGORY_ID_OTHER:
        return "Other";
    case ANCS_CATEGORY_ID_INCOMING_CALL:
        return "Incoming Call";
    case ANCS_CATEGORY_ID_MISSED_CALL:
        return "Missed Call";
    case ANCS_CATEGORY_ID_VOICEMAIL:
        return "Voicemail";
    case ANCS_CATEGORY_ID_SOCIAL:
        return "Social";
    case ANCS_CATEGORY_ID_SCHEDULE:
        return "Schedule";
    case ANCS_CATEGORY_ID_EMAIL:
        return "Email";
    case ANCS_CATEGORY_ID_NEWS:
        return "News";
    case ANCS_CATEGORY_ID_HEALTH_AND_FITNESS:
        return "Health";
    case ANCS_CATEGORY_ID_BUSINESS_AND_FINANCE:
        return "Finance";
    case ANCS_CATEGORY_ID_LOCATION:
        return "Location";
    case ANCS_CATEGORY_ID_ENTERTAINMENT:
        return "Entertainment";
    default:
        return "Unknown";
    }
}

esp_err_t ancs_parse_notification_source(const uint8_t *data, size_t len, ancs_notification_source_event_t *out_event)
{
    ESP_RETURN_ON_FALSE(data != NULL && out_event != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(len >= 8, ESP_ERR_INVALID_SIZE, TAG, "notification source packet too short");

    out_event->event_id = data[0];
    out_event->event_flags = data[1];
    out_event->category_id = data[2];
    out_event->category_count = data[3];
    out_event->notification_uid = (uint32_t)data[4] |
                                  ((uint32_t)data[5] << 8) |
                                  ((uint32_t)data[6] << 16) |
                                  ((uint32_t)data[7] << 24);
    return ESP_OK;
}

size_t ancs_build_get_notification_attrs_cmd(uint8_t *out_buf, size_t out_len, uint32_t uid,
                                             const ancs_attribute_request_t *attrs, size_t attr_count)
{
    size_t offset = 0;

    if (out_buf == NULL || attrs == NULL || attr_count == 0) {
        return 0;
    }

    if (out_len < 5) {
        return 0;
    }

    out_buf[offset++] = ANCS_COMMAND_ID_GET_NOTIFICATION_ATTRIBUTES;
    out_buf[offset++] = (uint8_t)(uid & 0xFF);
    out_buf[offset++] = (uint8_t)((uid >> 8) & 0xFF);
    out_buf[offset++] = (uint8_t)((uid >> 16) & 0xFF);
    out_buf[offset++] = (uint8_t)((uid >> 24) & 0xFF);

    for (size_t i = 0; i < attr_count; ++i) {
        if (offset + 1 > out_len) {
            return 0;
        }

        out_buf[offset++] = attrs[i].attribute_id;
        if (attrs[i].max_len > 0U) {
            if (offset + 2 > out_len) {
                return 0;
            }
            out_buf[offset++] = (uint8_t)(attrs[i].max_len & 0xFF);
            out_buf[offset++] = (uint8_t)((attrs[i].max_len >> 8) & 0xFF);
        }
    }

    return offset;
}

bool ancs_is_notification_attrs_complete(const uint8_t *data, size_t len,
                                         const ancs_attribute_request_t *attrs, size_t attr_count)
{
    size_t offset = 5;

    if (data == NULL || attrs == NULL || attr_count == 0 || len < 5) {
        return false;
    }

    if (data[0] != ANCS_COMMAND_ID_GET_NOTIFICATION_ATTRIBUTES) {
        return false;
    }

    for (size_t i = 0; i < attr_count; ++i) {
        uint16_t attr_len;
        if (offset + 3 > len) {
            return false;
        }

        attr_len = (uint16_t)data[offset + 1] | ((uint16_t)data[offset + 2] << 8);
        if (offset + 3 + attr_len > len) {
            return false;
        }
        offset += 3 + attr_len;
    }

    return true;
}

esp_err_t ancs_parse_notification_attrs(const uint8_t *data, size_t len, notification_record_t *out_record)
{
    size_t offset = 5;

    ESP_RETURN_ON_FALSE(data != NULL && out_record != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(len >= 5, ESP_ERR_INVALID_SIZE, TAG, "data source response too short");
    ESP_RETURN_ON_FALSE(data[0] == ANCS_COMMAND_ID_GET_NOTIFICATION_ATTRIBUTES, ESP_ERR_INVALID_RESPONSE, TAG,
                        "unexpected ANCS command response: %u", data[0]);

    out_record->uid = (uint32_t)data[1] |
                      ((uint32_t)data[2] << 8) |
                      ((uint32_t)data[3] << 16) |
                      ((uint32_t)data[4] << 24);

    while (offset + 3 <= len) {
        uint8_t attr_id = data[offset];
        uint16_t attr_len = (uint16_t)data[offset + 1] | ((uint16_t)data[offset + 2] << 8);
        const uint8_t *payload = &data[offset + 3];

        if (offset + 3 + attr_len > len) {
            ESP_LOGW(TAG, "attribute payload truncated, attr=%u len=%u", attr_id, attr_len);
            return ESP_ERR_INVALID_SIZE;
        }

        switch (attr_id) {
        case ANCS_ATTR_ID_APP_IDENTIFIER:
            ancs_safe_copy(out_record->app_id, sizeof(out_record->app_id), payload, attr_len);
            break;
        case ANCS_ATTR_ID_TITLE:
            ancs_safe_copy(out_record->title, sizeof(out_record->title), payload, attr_len);
            break;
        case ANCS_ATTR_ID_MESSAGE:
            ancs_safe_copy(out_record->message, sizeof(out_record->message), payload, attr_len);
            break;
        default:
            break;
        }

        offset += 3 + attr_len;
    }

    return ESP_OK;
}
