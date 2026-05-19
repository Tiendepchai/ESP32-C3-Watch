#pragma once

#include "driver/gpio.h"
#include "driver/i2c_types.h"
#include "hal/adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_DEVICE_NAME                    "C3-ANCS"

#define BOARD_I2C_PORT                       I2C_NUM_0
#define BOARD_I2C_SDA_GPIO                   GPIO_NUM_6
#define BOARD_I2C_SCL_GPIO                   GPIO_NUM_7
#define BOARD_I2C_CLOCK_HZ                   400000
#define BOARD_I2C_GLITCH_IGNORE_CNT          7

#define BOARD_SH1106_I2C_ADDRESS             0x3C
#define BOARD_SH1106_RESET_GPIO              ((gpio_num_t)-1)
#define BOARD_SH1106_WIDTH                   128
#define BOARD_SH1106_HEIGHT                  64
#define BOARD_SH1106_RAM_WIDTH               132
#define BOARD_SH1106_COLUMN_OFFSET           2
#define BOARD_SH1106_CONTRAST                0x80
#define BOARD_SH1106_MIRROR_X                1
#define BOARD_SH1106_MIRROR_Y                1

#define BOARD_NOTIFICATION_QUEUE_MAX         30
#define BOARD_ANCS_ATTR_QUEUE_MAX            10
#define BOARD_APP_EVENT_QUEUE_LEN            64
#define BOARD_APP_EVENT_SEND_WAIT_MS         10
#define BOARD_APP_FILTER_OVERLAY_MS          800
#define BOARD_APP_REBOOT_DELAY_MS            800
#define BOARD_ANCS_APP_ID_MAX_LEN            40
#define BOARD_ANCS_TITLE_MAX_LEN             64
#define BOARD_ANCS_MESSAGE_MAX_LEN           160
#define BOARD_NAV_SOURCE_MAX_LEN             32
#define BOARD_NAV_DISTANCE_MAX_LEN           24
#define BOARD_NAV_ETA_MAX_LEN                24
#define BOARD_NAV_PAYLOAD_MAX_LEN            384
#define BOARD_ANCS_DS_BUFFER_SIZE            384
#define BOARD_ANCS_PREEXISTING_ENRICH_MAX    4
#define BOARD_NOTIFICATION_APP_CONFIG_MAX    32
#define BOARD_NOTIFICATION_APP_PAGE_SIZE     4
#define BOARD_ANCS_ATTR_REQUEST_TIMEOUT_MS   2500
#define BOARD_ANCS_ATTR_RETRY_DELAY_MS       500
#define BOARD_ANCS_ATTR_MAX_RETRIES          2
#define BOARD_ANCS_DISCOVERY_DELAY_MS        3000
#define BOARD_ANCS_DISCOVERY_FIRST_BOND_MS   5000
#define BOARD_ANCS_DISCOVERY_RETRY_MS        3000
#define BOARD_ANCS_DISCOVERY_MAX_RETRIES     5
#define BOARD_BLE_DIRECTED_ADV_MS            5000
#define BOARD_BLE_RECONNECT_DELAY_MS         3000
#define BOARD_BLE_BOND_RECOVERY_ON_SEC_FAIL  1

#define BOARD_BUTTON_A_GPIO_PRIMARY          GPIO_NUM_3
#define BOARD_BUTTON_A_GPIO_SECONDARY        ((gpio_num_t)-1)
#define BOARD_BUTTON_B_GPIO_PRIMARY          GPIO_NUM_4
#define BOARD_BUTTON_B_GPIO_SECONDARY        ((gpio_num_t)-1)
#define BOARD_BUTTON_POLL_MS                 20
#define BOARD_BUTTON_DEBOUNCE_MS             40
#define BOARD_BUTTON_LONG_PRESS_MS           800
#define BOARD_BUTTON_AB_LONG_PRESS_MS        3000

#define BOARD_DISPLAY_FB_SIZE                (BOARD_SH1106_WIDTH * BOARD_SH1106_HEIGHT / 8)
#define BOARD_DISPLAY_AUTO_OFF_MS            15000

/* Battery ADC: voltage divider 2:1 (R_top:R_bot) → V_adc = V_bat / 3.
 * Reading 1400mV → 4.2V battery. ADC_ATTEN_DB_12 covers 0..3.1V. */
#define BOARD_BATTERY_ADC_GPIO               GPIO_NUM_1
#define BOARD_BATTERY_ADC_CHANNEL            ADC_CHANNEL_1
#define BOARD_BATTERY_ADC_UNIT               ADC_UNIT_1
#define BOARD_BATTERY_DIVIDER_NUM            3
#define BOARD_BATTERY_DIVIDER_DEN            1
#define BOARD_BATTERY_SAMPLE_COUNT           8

/* TP4056 STAT pins (open-drain, internal pull-up enabled).
 * CHRG=L → charging, STDBY=L → charge complete.
 * Both GPIO5 and GPIO10 are non-strapping on ESP32-C3 — boot is unaffected. */
#define BOARD_TP4056_CHRG_GPIO               GPIO_NUM_5
#define BOARD_TP4056_STDBY_GPIO              GPIO_NUM_10

#define BOARD_POWER_SAMPLE_MS                10000
#define BOARD_POWER_BATTERY_FULL_MV          4200
#define BOARD_POWER_BATTERY_EMPTY_MV         3300
#define BOARD_POWER_BATTERY_LOW_PCT          15
#define BOARD_POWER_BATTERY_CRITICAL_PCT     5

#define BOARD_TAG_STORAGE                    "storage_mgr"
#define BOARD_TAG_DISPLAY                    "display_mgr"
#define BOARD_TAG_TEXT                       "text_rndr"
#define BOARD_TAG_SH1106                     "sh1106"
#define BOARD_TAG_BLE                        "ble_mgr"
#define BOARD_TAG_ANCS                       "ancs_cli"
#define BOARD_TAG_ANCS_PARSER                "ancs_parse"
#define BOARD_TAG_STORE                      "notif_store"
#define BOARD_TAG_BUTTON                     "button_mgr"
#define BOARD_TAG_POWER                      "power_mgr"
#define BOARD_TAG_APP                        "app"

#ifdef __cplusplus
}
#endif
