idf_component_register(
    SRCS "notification_store.c"
    INCLUDE_DIRS "include"
    REQUIRES board_config freertos
)
