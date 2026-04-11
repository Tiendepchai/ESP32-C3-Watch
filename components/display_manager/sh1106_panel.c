#include "sh1106_panel.h"

#include <stdlib.h>
#include <sys/cdefs.h>
#include "board_config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_compiler.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = BOARD_TAG_SH1106;

#define SH1106_CMD_SET_LOWER_COLUMN_ADDR     0x00
#define SH1106_CMD_SET_HIGHER_COLUMN_ADDR    0x10
#define SH1106_CMD_SET_PUMP_VOLTAGE          0x30
#define SH1106_CMD_SET_DISPLAY_START_LINE    0x40
#define SH1106_CMD_SET_CONTRAST              0x81
#define SH1106_CMD_SEG_REMAP_NORMAL          0xA0
#define SH1106_CMD_SEG_REMAP_MIRROR          0xA1
#define SH1106_CMD_ENTIRE_DISPLAY_RESUME     0xA4
#define SH1106_CMD_INVERT_OFF                0xA6
#define SH1106_CMD_INVERT_ON                 0xA7
#define SH1106_CMD_SET_MULTIPLEX             0xA8
#define SH1106_CMD_DC_DC_CONTROL             0xAD
#define SH1106_CMD_DISPLAY_OFF               0xAE
#define SH1106_CMD_DISPLAY_ON                0xAF
#define SH1106_CMD_SET_PAGE_ADDR             0xB0
#define SH1106_CMD_COM_SCAN_NORMAL           0xC0
#define SH1106_CMD_COM_SCAN_MIRROR           0xC8
#define SH1106_CMD_SET_DISPLAY_OFFSET        0xD3
#define SH1106_CMD_SET_DISPLAY_CLOCK_DIV     0xD5
#define SH1106_CMD_SET_PRECHARGE             0xD9
#define SH1106_CMD_SET_COM_PINS              0xDA
#define SH1106_CMD_SET_VCOM_DESELECT         0xDB

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    gpio_num_t reset_gpio_num;
    uint8_t width;
    uint8_t height;
    uint8_t contrast;
    uint8_t column_offset;
    int x_gap;
    int y_gap;
    bool reset_level;
    bool mirror_x;
    bool mirror_y;
} sh1106_panel_t;

static esp_err_t panel_sh1106_del(esp_lcd_panel_t *panel);
static esp_err_t panel_sh1106_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_sh1106_init(esp_lcd_panel_t *panel);
static esp_err_t panel_sh1106_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_sh1106_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_sh1106_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_sh1106_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_sh1106_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_sh1106_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

static esp_err_t panel_sh1106_set_column_page(sh1106_panel_t *sh1106, uint8_t page, uint8_t column)
{
    esp_lcd_panel_io_handle_t io = sh1106->io;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_SET_PAGE_ADDR | (page & 0x0F), NULL, 0),
                        TAG, "set page address failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_SET_LOWER_COLUMN_ADDR | (column & 0x0F), NULL, 0),
                        TAG, "set lower column failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_SET_HIGHER_COLUMN_ADDR | ((column >> 4) & 0x0F), NULL, 0),
                        TAG, "set higher column failed");
    return ESP_OK;
}

static esp_err_t panel_sh1106_clear_ram(sh1106_panel_t *sh1106)
{
    uint8_t blank_line[BOARD_SH1106_WIDTH] = {0};
    uint8_t pages = sh1106->height / 8U;

    for (uint8_t page = 0; page < pages; ++page) {
        ESP_RETURN_ON_ERROR(panel_sh1106_set_column_page(sh1106, page, sh1106->column_offset), TAG, "set cursor failed");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(sh1106->io, -1, blank_line, sizeof(blank_line)),
                            TAG, "clear page failed");
    }
    return ESP_OK;
}

esp_err_t esp_lcd_new_panel_sh1106(const esp_lcd_panel_io_handle_t io,
                                   const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    sh1106_panel_t *sh1106 = NULL;
    esp_lcd_panel_sh1106_config_t *vendor_config = NULL;

    ESP_GOTO_ON_FALSE(io != NULL && panel_dev_config != NULL && ret_panel != NULL, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    ESP_GOTO_ON_FALSE(panel_dev_config->bits_per_pixel == 1, ESP_ERR_INVALID_ARG, err, TAG, "SH1106 requires 1bpp");

    vendor_config = (esp_lcd_panel_sh1106_config_t *)panel_dev_config->vendor_config;
    ESP_GOTO_ON_FALSE(vendor_config != NULL, ESP_ERR_INVALID_ARG, err, TAG, "vendor_config is required");

    ESP_COMPILER_DIAGNOSTIC_PUSH_IGNORE("-Wanalyzer-malloc-leak")
    sh1106 = calloc(1, sizeof(*sh1106));
    ESP_GOTO_ON_FALSE(sh1106 != NULL, ESP_ERR_NO_MEM, err, TAG, "no mem for sh1106 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "reset gpio config failed");
    }

    sh1106->io = io;
    sh1106->reset_gpio_num = panel_dev_config->reset_gpio_num;
    sh1106->reset_level = panel_dev_config->flags.reset_active_high;
    sh1106->width = vendor_config->width;
    sh1106->height = vendor_config->height;
    sh1106->contrast = vendor_config->contrast;
    sh1106->column_offset = vendor_config->column_offset;
    sh1106->mirror_x = vendor_config->mirror_x;
    sh1106->mirror_y = vendor_config->mirror_y;

    sh1106->base.del = panel_sh1106_del;
    sh1106->base.reset = panel_sh1106_reset;
    sh1106->base.init = panel_sh1106_init;
    sh1106->base.draw_bitmap = panel_sh1106_draw_bitmap;
    sh1106->base.invert_color = panel_sh1106_invert_color;
    sh1106->base.mirror = panel_sh1106_mirror;
    sh1106->base.swap_xy = panel_sh1106_swap_xy;
    sh1106->base.set_gap = panel_sh1106_set_gap;
    sh1106->base.disp_on_off = panel_sh1106_disp_on_off;

    *ret_panel = &sh1106->base;
    return ESP_OK;

err:
    if (sh1106 != NULL) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(sh1106);
    }
    return ret;
    ESP_COMPILER_DIAGNOSTIC_POP("-Wanalyzer-malloc-leak")
}

static esp_err_t panel_sh1106_del(esp_lcd_panel_t *panel)
{
    sh1106_panel_t *sh1106 = __containerof(panel, sh1106_panel_t, base);
    if (sh1106->reset_gpio_num >= 0) {
        gpio_reset_pin(sh1106->reset_gpio_num);
    }
    free(sh1106);
    return ESP_OK;
}

static esp_err_t panel_sh1106_reset(esp_lcd_panel_t *panel)
{
    sh1106_panel_t *sh1106 = __containerof(panel, sh1106_panel_t, base);

    if (sh1106->reset_gpio_num >= 0) {
        gpio_set_level(sh1106->reset_gpio_num, sh1106->reset_level);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level(sh1106->reset_gpio_num, !sh1106->reset_level);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return ESP_OK;
}

static esp_err_t panel_sh1106_init(esp_lcd_panel_t *panel)
{
    sh1106_panel_t *sh1106 = __containerof(panel, sh1106_panel_t, base);
    esp_lcd_panel_io_handle_t io = sh1106->io;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_DISPLAY_OFF, NULL, 0), TAG, "display off failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_SET_DISPLAY_CLOCK_DIV, (uint8_t[]){ 0x80 }, 1),
                        TAG, "clock div failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_SET_MULTIPLEX, (uint8_t[]){ sh1106->height - 1U }, 1),
                        TAG, "multiplex failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_SET_DISPLAY_OFFSET, (uint8_t[]){ 0x00 }, 1),
                        TAG, "display offset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_SET_DISPLAY_START_LINE | 0x00, NULL, 0),
                        TAG, "start line failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_SET_PUMP_VOLTAGE | 0x02, NULL, 0),
                        TAG, "pump voltage failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_DC_DC_CONTROL, (uint8_t[]){ 0x8B }, 1),
                        TAG, "dc-dc enable failed");
    ESP_RETURN_ON_ERROR(panel_sh1106_mirror(panel, sh1106->mirror_x, sh1106->mirror_y), TAG, "mirror config failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_SET_CONTRAST, (uint8_t[]){ sh1106->contrast }, 1),
                        TAG, "contrast failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_SET_PRECHARGE, (uint8_t[]){ 0x22 }, 1),
                        TAG, "precharge failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_SET_COM_PINS,
                                                  (uint8_t[]){ sh1106->height == 64U ? 0x12 : 0x02 }, 1),
                        TAG, "COM pins failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_SET_VCOM_DESELECT, (uint8_t[]){ 0x35 }, 1),
                        TAG, "VCOM deselect failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_ENTIRE_DISPLAY_RESUME, NULL, 0),
                        TAG, "resume display failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_INVERT_OFF, NULL, 0), TAG, "normal display failed");
    ESP_RETURN_ON_ERROR(panel_sh1106_clear_ram(sh1106), TAG, "clear ram failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SH1106_CMD_DISPLAY_ON, NULL, 0), TAG, "display on failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

static esp_err_t panel_sh1106_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    sh1106_panel_t *sh1106 = __containerof(panel, sh1106_panel_t, base);
    const uint8_t *data = (const uint8_t *)color_data;
    int width;
    int page_start;
    int page_end;

    ESP_RETURN_ON_FALSE(color_data != NULL, ESP_ERR_INVALID_ARG, TAG, "color_data is null");
    ESP_RETURN_ON_FALSE(x_start >= 0 && y_start >= 0 && x_end <= sh1106->width && y_end <= sh1106->height,
                        ESP_ERR_INVALID_ARG, TAG, "draw window out of range");
    ESP_RETURN_ON_FALSE((y_start % 8) == 0 && (y_end % 8) == 0, ESP_ERR_NOT_SUPPORTED, TAG,
                        "SH1106 draw requires page-aligned y");

    width = x_end - x_start;
    page_start = (y_start + sh1106->y_gap) / 8;
    page_end = (y_end + sh1106->y_gap) / 8;

    for (int page = page_start; page < page_end; ++page) {
        uint8_t column = (uint8_t)(x_start + sh1106->x_gap + sh1106->column_offset);
        ESP_RETURN_ON_ERROR(panel_sh1106_set_column_page(sh1106, (uint8_t)page, column), TAG, "set cursor failed");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(sh1106->io, -1, data, (size_t)width), TAG, "page transfer failed");
        data += width;
    }

    return ESP_OK;
}

static esp_err_t panel_sh1106_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    sh1106_panel_t *sh1106 = __containerof(panel, sh1106_panel_t, base);
    int cmd = invert_color_data ? SH1106_CMD_INVERT_ON : SH1106_CMD_INVERT_OFF;
    return esp_lcd_panel_io_tx_param(sh1106->io, cmd, NULL, 0);
}

static esp_err_t panel_sh1106_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    sh1106_panel_t *sh1106 = __containerof(panel, sh1106_panel_t, base);
    int seg_cmd = mirror_x ? SH1106_CMD_SEG_REMAP_MIRROR : SH1106_CMD_SEG_REMAP_NORMAL;
    int com_cmd = mirror_y ? SH1106_CMD_COM_SCAN_MIRROR : SH1106_CMD_COM_SCAN_NORMAL;

    sh1106->mirror_x = mirror_x;
    sh1106->mirror_y = mirror_y;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(sh1106->io, seg_cmd, NULL, 0), TAG, "segment remap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(sh1106->io, com_cmd, NULL, 0), TAG, "COM scan failed");
    return ESP_OK;
}

static esp_err_t panel_sh1106_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    (void)panel;
    (void)swap_axes;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t panel_sh1106_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    sh1106_panel_t *sh1106 = __containerof(panel, sh1106_panel_t, base);
    sh1106->x_gap = x_gap;
    sh1106->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_sh1106_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    sh1106_panel_t *sh1106 = __containerof(panel, sh1106_panel_t, base);
    int cmd = on_off ? SH1106_CMD_DISPLAY_ON : SH1106_CMD_DISPLAY_OFF;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(sh1106->io, cmd, NULL, 0), TAG, "display on/off failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}
