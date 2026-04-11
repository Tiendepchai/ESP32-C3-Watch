#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t contrast;
    uint8_t column_offset;
    bool mirror_x;
    bool mirror_y;
} esp_lcd_panel_sh1106_config_t;

esp_err_t esp_lcd_new_panel_sh1106(const esp_lcd_panel_io_handle_t io,
                                   const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif
