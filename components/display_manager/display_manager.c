#include "display_manager.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "board_config.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sh1106_panel.h"
#include "text_renderer.h"

struct display_manager {
    i2c_master_bus_handle_t i2c_bus;
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_handle_t panel_handle;
    uint8_t framebuffer[BOARD_DISPLAY_FB_SIZE];
    SemaphoreHandle_t lock;
};

static const char *TAG = BOARD_TAG_DISPLAY;
static const uint16_t s_probe_addresses[] = {
    BOARD_SH1106_I2C_ADDRESS,
    0x3C,
    0x3D,
};

typedef struct {
    const char *bundle_id;
    const char *display_name;
} display_app_name_map_t;

static const display_app_name_map_t s_app_name_map[] = {
    { "com.apple.mobilephone", "Phone" },
    { "com.apple.MobileSMS", "Messages" },
    { "com.apple.mobilemail", "Mail" },
    { "com.apple.Preferences", "Settings" },
    { "com.apple.Maps", "Maps" },
    { "com.apple.calculator", "Calculator" },
    { "com.apple.weather", "Weather" },
    { "com.google.Gmail", "Gmail" },
    { "com.facebook.Messenger", "Messenger" },
    { "net.whatsapp.WhatsApp", "WhatsApp" },
    { "org.telegram.messenger", "Telegram" },
    { "org.whispersystems.signal", "Signal" },
};

static esp_err_t display_flush_locked(display_handle_t handle)
{
    return esp_lcd_panel_draw_bitmap(handle->panel_handle, 0, 0,
                                     BOARD_SH1106_WIDTH, BOARD_SH1106_HEIGHT,
                                     handle->framebuffer);
}

static uint16_t display_select_i2c_address(i2c_master_bus_handle_t bus)
{
    uint16_t detected_address = BOARD_SH1106_I2C_ADDRESS;

    for (size_t i = 0; i < sizeof(s_probe_addresses) / sizeof(s_probe_addresses[0]); ++i) {
        uint16_t address = s_probe_addresses[i];

        if (i > 0) {
            bool duplicate = false;

            for (size_t j = 0; j < i; ++j) {
                if (s_probe_addresses[j] == address) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                continue;
            }
        }

        if (i2c_master_probe(bus, address, 50) == ESP_OK) {
            ESP_LOGI(TAG, "I2C device acknowledged at 0x%02X", address);
            detected_address = address;
            if (address != BOARD_SH1106_I2C_ADDRESS) {
                ESP_LOGW(TAG, "configured SH1106 address 0x%02X not found, using 0x%02X instead",
                         BOARD_SH1106_I2C_ADDRESS, address);
            }
            return detected_address;
        }

        ESP_LOGW(TAG, "no I2C ACK at 0x%02X", address);
    }

    ESP_LOGW(TAG, "no SH1106 detected on I2C, keeping configured address 0x%02X",
             BOARD_SH1106_I2C_ADDRESS);
    return detected_address;
}

static const char *display_safe_text(const char *text)
{
    return (text != NULL && text[0] != '\0') ? text : "";
}

static size_t display_utf8_sequence_len(uint8_t lead)
{
    if ((lead & 0x80U) == 0U) {
        return 1;
    }
    if ((lead & 0xE0U) == 0xC0U) {
        return 2;
    }
    if ((lead & 0xF0U) == 0xE0U) {
        return 3;
    }
    if ((lead & 0xF8U) == 0xF0U) {
        return 4;
    }
    return 1;
}

static bool display_decode_utf8_char(const uint8_t *in, size_t *consumed, uint32_t *codepoint)
{
    size_t len;

    if (in == NULL || consumed == NULL || codepoint == NULL || in[0] == 0U) {
        return false;
    }

    len = display_utf8_sequence_len(in[0]);
    *consumed = len;

    switch (len) {
    case 1:
        *codepoint = in[0];
        return true;
    case 2:
        if ((in[1] & 0xC0U) != 0x80U) {
            *codepoint = in[0];
            *consumed = 1;
            return false;
        }
        *codepoint = ((uint32_t)(in[0] & 0x1FU) << 6) |
                     ((uint32_t)(in[1] & 0x3FU));
        return true;
    case 3:
        if ((in[1] & 0xC0U) != 0x80U || (in[2] & 0xC0U) != 0x80U) {
            *codepoint = in[0];
            *consumed = 1;
            return false;
        }
        *codepoint = ((uint32_t)(in[0] & 0x0FU) << 12) |
                     ((uint32_t)(in[1] & 0x3FU) << 6) |
                     ((uint32_t)(in[2] & 0x3FU));
        return true;
    case 4:
        if ((in[1] & 0xC0U) != 0x80U || (in[2] & 0xC0U) != 0x80U || (in[3] & 0xC0U) != 0x80U) {
            *codepoint = in[0];
            *consumed = 1;
            return false;
        }
        *codepoint = ((uint32_t)(in[0] & 0x07U) << 18) |
                     ((uint32_t)(in[1] & 0x3FU) << 12) |
                     ((uint32_t)(in[2] & 0x3FU) << 6) |
                     ((uint32_t)(in[3] & 0x3FU));
        return true;
    default:
        *codepoint = in[0];
        *consumed = 1;
        return false;
    }
}

static void display_append_char(char *out, size_t out_size, size_t *offset, char c)
{
    if (out == NULL || offset == NULL || out_size == 0 || *offset >= (out_size - 1U)) {
        return;
    }
    out[(*offset)++] = c;
    out[*offset] = '\0';
}

static void display_append_text(char *out, size_t out_size, size_t *offset, const char *text)
{
    if (text == NULL) {
        return;
    }

    while (*text != '\0' && *offset < (out_size - 1U)) {
        out[(*offset)++] = *text++;
    }
    out[*offset] = '\0';
}

static const char *display_transliterate_codepoint(uint32_t cp)
{
    switch (cp) {
    case 0x00A0:
        return " ";
    case 0x2018:
    case 0x2019:
    case 0x201A:
    case 0x201B:
        return "'";
    case 0x201C:
    case 0x201D:
    case 0x201E:
    case 0x201F:
        return "\"";
    case 0x2013:
    case 0x2014:
    case 0x2212:
        return "-";
    case 0x2026:
        return "...";
    case 0x00C0:
    case 0x00C1:
    case 0x00C2:
    case 0x00C3:
    case 0x00C4:
    case 0x00C5:
    case 0x0102:
    case 0x1EA0:
    case 0x1EA2:
    case 0x1EA4:
    case 0x1EA6:
    case 0x1EA8:
    case 0x1EAA:
    case 0x1EAC:
    case 0x1EAE:
    case 0x1EB0:
    case 0x1EB2:
    case 0x1EB4:
    case 0x1EB6:
        return "A";
    case 0x00E0:
    case 0x00E1:
    case 0x00E2:
    case 0x00E3:
    case 0x00E4:
    case 0x00E5:
    case 0x0103:
    case 0x1EA1:
    case 0x1EA3:
    case 0x1EA5:
    case 0x1EA7:
    case 0x1EA9:
    case 0x1EAB:
    case 0x1EAD:
    case 0x1EAF:
    case 0x1EB1:
    case 0x1EB3:
    case 0x1EB5:
    case 0x1EB7:
        return "a";
    case 0x00C7:
        return "C";
    case 0x00E7:
        return "c";
    case 0x00D0:
    case 0x0110:
        return "D";
    case 0x00F0:
    case 0x0111:
        return "d";
    case 0x00C8:
    case 0x00C9:
    case 0x00CA:
    case 0x00CB:
    case 0x1EB8:
    case 0x1EBA:
    case 0x1EBC:
    case 0x1EBE:
    case 0x1EC0:
    case 0x1EC2:
    case 0x1EC4:
    case 0x1EC6:
        return "E";
    case 0x00E8:
    case 0x00E9:
    case 0x00EA:
    case 0x00EB:
    case 0x1EB9:
    case 0x1EBB:
    case 0x1EBD:
    case 0x1EBF:
    case 0x1EC1:
    case 0x1EC3:
    case 0x1EC5:
    case 0x1EC7:
        return "e";
    case 0x00CC:
    case 0x00CD:
    case 0x00CE:
    case 0x00CF:
    case 0x1EC8:
    case 0x1ECA:
        return "I";
    case 0x00EC:
    case 0x00ED:
    case 0x00EE:
    case 0x00EF:
    case 0x1EC9:
    case 0x1ECB:
        return "i";
    case 0x00D1:
        return "N";
    case 0x00F1:
        return "n";
    case 0x00D2:
    case 0x00D3:
    case 0x00D4:
    case 0x00D5:
    case 0x00D6:
    case 0x00D8:
    case 0x01A0:
    case 0x1ECC:
    case 0x1ECE:
    case 0x1ED0:
    case 0x1ED2:
    case 0x1ED4:
    case 0x1ED6:
    case 0x1ED8:
    case 0x1EDA:
    case 0x1EDC:
    case 0x1EDE:
    case 0x1EE0:
    case 0x1EE2:
        return "O";
    case 0x00F2:
    case 0x00F3:
    case 0x00F4:
    case 0x00F5:
    case 0x00F6:
    case 0x00F8:
    case 0x01A1:
    case 0x1ECD:
    case 0x1ECF:
    case 0x1ED1:
    case 0x1ED3:
    case 0x1ED5:
    case 0x1ED7:
    case 0x1ED9:
    case 0x1EDB:
    case 0x1EDD:
    case 0x1EDF:
    case 0x1EE1:
    case 0x1EE3:
        return "o";
    case 0x00D9:
    case 0x00DA:
    case 0x00DB:
    case 0x00DC:
    case 0x01AF:
    case 0x1EE4:
    case 0x1EE6:
    case 0x1EE8:
    case 0x1EEA:
    case 0x1EEC:
    case 0x1EEE:
    case 0x1EF0:
        return "U";
    case 0x00F9:
    case 0x00FA:
    case 0x00FB:
    case 0x00FC:
    case 0x01B0:
    case 0x1EE5:
    case 0x1EE7:
    case 0x1EE9:
    case 0x1EEB:
    case 0x1EED:
    case 0x1EEF:
    case 0x1EF1:
        return "u";
    case 0x00DD:
    case 0x0178:
    case 0x1EF2:
    case 0x1EF4:
    case 0x1EF6:
    case 0x1EF8:
        return "Y";
    case 0x00FD:
    case 0x00FF:
    case 0x1EF3:
    case 0x1EF5:
    case 0x1EF7:
    case 0x1EF9:
        return "y";
    default:
        if (cp >= 0x0300U && cp <= 0x036FU) {
            return "";
        }
        return NULL;
    }
}

static void display_sanitize_text(const char *input, char *out, size_t out_size)
{
    const uint8_t *p = (const uint8_t *)display_safe_text(input);
    size_t offset = 0;
    bool last_was_space = false;

    if (out == NULL || out_size == 0) {
        return;
    }

    out[0] = '\0';

    while (*p != 0U && offset < (out_size - 1U)) {
        uint32_t cp = 0;
        size_t consumed = 1;
        const char *replacement = NULL;

        display_decode_utf8_char(p, &consumed, &cp);
        p += consumed;

        if (cp < 0x80U) {
            char c = (char)cp;

            if (c == '\r' || c == '\n' || c == '\t') {
                c = ' ';
            }

            if ((unsigned char)c < 32U || (unsigned char)c > 126U) {
                continue;
            }

            if (c == ' ') {
                if (last_was_space) {
                    continue;
                }
                last_was_space = true;
            } else {
                last_was_space = false;
            }

            display_append_char(out, out_size, &offset, c);
            continue;
        }

        replacement = display_transliterate_codepoint(cp);
        if (replacement == NULL) {
            continue;
        }
        if (replacement[0] == '\0') {
            continue;
        }
        if (strcmp(replacement, " ") == 0) {
            if (last_was_space) {
                continue;
            }
            last_was_space = true;
        } else {
            last_was_space = false;
        }
        display_append_text(out, out_size, &offset, replacement);
    }
}

static void display_title_case_words(char *text)
{
    bool capitalize = true;

    if (text == NULL) {
        return;
    }

    for (char *p = text; *p != '\0'; ++p) {
        if (*p == ' ') {
            capitalize = true;
            continue;
        }
        if (capitalize) {
            *p = (char)toupper((unsigned char)*p);
            capitalize = false;
        } else {
            *p = (char)tolower((unsigned char)*p);
        }
    }
}

static void display_format_bundle_label(const char *bundle_id, char *out, size_t out_size)
{
    const char *segment = bundle_id;

    if (out == NULL || out_size == 0) {
        return;
    }

    for (size_t i = 0; i < sizeof(s_app_name_map) / sizeof(s_app_name_map[0]); ++i) {
        if (strcmp(bundle_id, s_app_name_map[i].bundle_id) == 0) {
            strlcpy(out, s_app_name_map[i].display_name, out_size);
            return;
        }
    }

    for (const char *p = bundle_id; *p != '\0'; ++p) {
        if (*p == '.') {
            segment = p + 1;
        }
    }

    strlcpy(out, segment, out_size);
    for (char *p = out; *p != '\0'; ++p) {
        if (*p == '.' || *p == '_' || *p == '-') {
            *p = ' ';
        }
    }
    display_title_case_words(out);
}

static void display_prepare_app_label(const char *app, char *out, size_t out_size)
{
    const char *safe_app = display_safe_text(app);

    if (out == NULL || out_size == 0) {
        return;
    }

    if (safe_app[0] == '\0') {
        strlcpy(out, "Notification", out_size);
        return;
    }

    if (strchr(safe_app, '.') != NULL) {
        display_format_bundle_label(safe_app, out, out_size);
        return;
    }

    display_sanitize_text(safe_app, out, out_size);
    if (out[0] == '\0') {
        strlcpy(out, "Notification", out_size);
    }
}

esp_err_t display_init(display_handle_t *out_handle)
{
    esp_err_t ret = ESP_OK;
    display_handle_t handle = NULL;
    i2c_master_bus_config_t bus_config = {0};
    esp_lcd_panel_io_i2c_config_t io_config = {0};
    esp_lcd_panel_dev_config_t panel_config = {0};
    esp_lcd_panel_sh1106_config_t sh1106_config = {0};

    ESP_RETURN_ON_FALSE(out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "out_handle is null");

    handle = calloc(1, sizeof(*handle));
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_NO_MEM, TAG, "no memory for display handle");

    handle->lock = xSemaphoreCreateMutex();
    if (handle->lock == NULL) {
        free(handle);
        return ESP_ERR_NO_MEM;
    }

    bus_config.i2c_port = BOARD_I2C_PORT;
    bus_config.sda_io_num = BOARD_I2C_SDA_GPIO;
    bus_config.scl_io_num = BOARD_I2C_SCL_GPIO;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = BOARD_I2C_GLITCH_IGNORE_CNT;
    bus_config.flags.enable_internal_pullup = true;
    ESP_GOTO_ON_ERROR(i2c_new_master_bus(&bus_config, &handle->i2c_bus), err, TAG, "failed to create I2C bus");

    io_config.dev_addr = display_select_i2c_address(handle->i2c_bus);
    io_config.scl_speed_hz = BOARD_I2C_CLOCK_HZ;
    io_config.control_phase_bytes = 1;
    io_config.dc_bit_offset = 6;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_i2c(handle->i2c_bus, &io_config, &handle->io_handle), err, TAG,
                      "failed to create panel io");

    sh1106_config.width = BOARD_SH1106_WIDTH;
    sh1106_config.height = BOARD_SH1106_HEIGHT;
    sh1106_config.contrast = BOARD_SH1106_CONTRAST;
    sh1106_config.column_offset = BOARD_SH1106_COLUMN_OFFSET;
    sh1106_config.mirror_x = BOARD_SH1106_MIRROR_X != 0;
    sh1106_config.mirror_y = BOARD_SH1106_MIRROR_Y != 0;

    panel_config.bits_per_pixel = 1;
    panel_config.reset_gpio_num = BOARD_SH1106_RESET_GPIO;
    panel_config.vendor_config = &sh1106_config;
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_sh1106(handle->io_handle, &panel_config, &handle->panel_handle), err, TAG,
                      "failed to create SH1106 panel");

    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(handle->panel_handle), err, TAG, "panel reset failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(handle->panel_handle), err, TAG, "panel init failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_disp_on_off(handle->panel_handle, true), err, TAG, "panel on failed");

    text_renderer_clear(handle->framebuffer, sizeof(handle->framebuffer), false);
    ESP_GOTO_ON_ERROR(display_flush_locked(handle), err, TAG, "initial clear flush failed");

    *out_handle = handle;
    ESP_LOGI(TAG, "display initialized on SDA GPIO%d, SCL GPIO%d, address 0x%02X",
             BOARD_I2C_SDA_GPIO, BOARD_I2C_SCL_GPIO, io_config.dev_addr);
    return ESP_OK;

err:
    display_deinit(handle);
    return ret;
}

void display_deinit(display_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    if (handle->panel_handle != NULL) {
        esp_lcd_panel_del(handle->panel_handle);
    }
    if (handle->io_handle != NULL) {
        esp_lcd_panel_io_del(handle->io_handle);
    }
    if (handle->i2c_bus != NULL) {
        i2c_del_master_bus(handle->i2c_bus);
    }
    if (handle->lock != NULL) {
        vSemaphoreDelete(handle->lock);
    }
    free(handle);
}

esp_err_t display_clear(display_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    xSemaphoreTake(handle->lock, portMAX_DELAY);
    text_renderer_clear(handle->framebuffer, sizeof(handle->framebuffer), false);
    esp_err_t rc = display_flush_locked(handle);
    xSemaphoreGive(handle->lock);
    return rc;
}

esp_err_t display_set_inverted(display_handle_t handle, bool inverted)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    return esp_lcd_panel_invert_color(handle->panel_handle, inverted);
}

esp_err_t display_show_status(display_handle_t handle, const char *status)
{
    text_renderer_box_t status_box = {
        .x = 0,
        .y = 24,
        .width = BOARD_SH1106_WIDTH,
        .height = 24,
    };

    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    text_renderer_clear(handle->framebuffer, sizeof(handle->framebuffer), false);
    text_renderer_draw_string(handle->framebuffer, BOARD_SH1106_WIDTH, BOARD_SH1106_HEIGHT, 0, 0, "ESP32-C3 ANCS");
    text_renderer_draw_wrapped_text(handle->framebuffer, BOARD_SH1106_WIDTH, BOARD_SH1106_HEIGHT,
                                    &status_box, display_safe_text(status), false);
    esp_err_t rc = display_flush_locked(handle);
    xSemaphoreGive(handle->lock);
    return rc;
}

esp_err_t display_show_notification(display_handle_t handle, const char *app, const char *title, const char *message)
{
    text_renderer_box_t message_box = {
        .x = 0,
        .y = 24,
        .width = BOARD_SH1106_WIDTH,
        .height = BOARD_SH1106_HEIGHT - 24,
    };
    char line0[BOARD_ANCS_APP_ID_MAX_LEN];
    char line1[BOARD_ANCS_TITLE_MAX_LEN];
    char line2[BOARD_ANCS_MESSAGE_MAX_LEN];

    display_prepare_app_label(app, line0, sizeof(line0));
    display_sanitize_text(title, line1, sizeof(line1));
    display_sanitize_text(message, line2, sizeof(line2));

    if (line1[0] == '\0') {
        strlcpy(line1, "(no title)", sizeof(line1));
    }

    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    text_renderer_clear(handle->framebuffer, sizeof(handle->framebuffer), false);
    text_renderer_draw_string(handle->framebuffer, BOARD_SH1106_WIDTH, BOARD_SH1106_HEIGHT, 0, 0, line0);
    text_renderer_draw_string(handle->framebuffer, BOARD_SH1106_WIDTH, BOARD_SH1106_HEIGHT, 0, 8, line1);
    text_renderer_draw_wrapped_text(handle->framebuffer, BOARD_SH1106_WIDTH, BOARD_SH1106_HEIGHT,
                                    &message_box, line2, true);
    esp_err_t rc = display_flush_locked(handle);
    xSemaphoreGive(handle->lock);
    return rc;
}
