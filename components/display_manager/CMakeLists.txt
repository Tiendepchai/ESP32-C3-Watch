idf_component_register(
    SRCS "display_manager.c" "sh1106_panel.c" "text_renderer.c"
    INCLUDE_DIRS "include"
    REQUIRES board_config esp_lcd esp_driver_i2c esp_driver_gpio freertos
)
