idf_component_register(
    SRCS "ancs_client.c" "ancs_parser.c"
    INCLUDE_DIRS "include"
    REQUIRES board_config notification_store bt esp_timer freertos
)
