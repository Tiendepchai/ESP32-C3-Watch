idf_component_register(
    SRCS "button_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES board_config esp_driver_gpio esp_timer freertos
)
