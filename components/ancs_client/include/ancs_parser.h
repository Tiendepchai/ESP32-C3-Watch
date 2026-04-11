#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "notification_store.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ANCS_EVENT_ID_NOTIFICATION_ADDED = 0,
    ANCS_EVENT_ID_NOTIFICATION_MODIFIED = 1,
    ANCS_EVENT_ID_NOTIFICATION_REMOVED = 2,
} ancs_event_id_t;

typedef enum {
    ANCS_EVENT_FLAG_SILENT = (1 << 0),
    ANCS_EVENT_FLAG_IMPORTANT = (1 << 1),
    ANCS_EVENT_FLAG_PRE_EXISTING = (1 << 2),
    ANCS_EVENT_FLAG_POSITIVE_ACTION = (1 << 3),
    ANCS_EVENT_FLAG_NEGATIVE_ACTION = (1 << 4),
} ancs_event_flag_t;

typedef enum {
    ANCS_CATEGORY_ID_OTHER = 0,
    ANCS_CATEGORY_ID_INCOMING_CALL = 1,
    ANCS_CATEGORY_ID_MISSED_CALL = 2,
    ANCS_CATEGORY_ID_VOICEMAIL = 3,
    ANCS_CATEGORY_ID_SOCIAL = 4,
    ANCS_CATEGORY_ID_SCHEDULE = 5,
    ANCS_CATEGORY_ID_EMAIL = 6,
    ANCS_CATEGORY_ID_NEWS = 7,
    ANCS_CATEGORY_ID_HEALTH_AND_FITNESS = 8,
    ANCS_CATEGORY_ID_BUSINESS_AND_FINANCE = 9,
    ANCS_CATEGORY_ID_LOCATION = 10,
    ANCS_CATEGORY_ID_ENTERTAINMENT = 11,
} ancs_category_id_t;

typedef enum {
    ANCS_COMMAND_ID_GET_NOTIFICATION_ATTRIBUTES = 0,
    ANCS_COMMAND_ID_GET_APP_ATTRIBUTES = 1,
    ANCS_COMMAND_ID_PERFORM_NOTIFICATION_ACTION = 2,
} ancs_command_id_t;

typedef enum {
    ANCS_ATTR_ID_APP_IDENTIFIER = 0,
    ANCS_ATTR_ID_TITLE = 1,
    ANCS_ATTR_ID_SUBTITLE = 2,
    ANCS_ATTR_ID_MESSAGE = 3,
    ANCS_ATTR_ID_MESSAGE_SIZE = 4,
    ANCS_ATTR_ID_DATE = 5,
    ANCS_ATTR_ID_POSITIVE_ACTION_LABEL = 6,
    ANCS_ATTR_ID_NEGATIVE_ACTION_LABEL = 7,
} ancs_attribute_id_t;

typedef struct {
    uint8_t attribute_id;
    uint16_t max_len;
} ancs_attribute_request_t;

typedef struct {
    uint8_t event_id;
    uint8_t event_flags;
    uint8_t category_id;
    uint8_t category_count;
    uint32_t notification_uid;
} ancs_notification_source_event_t;

const char *ancs_event_id_to_string(uint8_t event_id);
const char *ancs_category_id_to_string(uint8_t category_id);

esp_err_t ancs_parse_notification_source(const uint8_t *data, size_t len, ancs_notification_source_event_t *out_event);
size_t ancs_build_get_notification_attrs_cmd(uint8_t *out_buf, size_t out_len, uint32_t uid,
                                             const ancs_attribute_request_t *attrs, size_t attr_count);
bool ancs_is_notification_attrs_complete(const uint8_t *data, size_t len,
                                         const ancs_attribute_request_t *attrs, size_t attr_count);
esp_err_t ancs_parse_notification_attrs(const uint8_t *data, size_t len, notification_record_t *out_record);

#ifdef __cplusplus
}
#endif
