idf_component_register(
    SRCS "main.c" "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES board_config ble_manager button_manager cts_client display_manager notification_store power_manager storage_manager
)
