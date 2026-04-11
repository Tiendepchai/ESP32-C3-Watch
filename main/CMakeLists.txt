idf_component_register(
    SRCS "main.c" "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES board_config ble_manager button_manager display_manager notification_store storage_manager
)
