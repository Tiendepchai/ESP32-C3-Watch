idf_component_register(
    SRCS "ble_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES board_config ancs_client bt freertos esp_timer storage_manager
)
