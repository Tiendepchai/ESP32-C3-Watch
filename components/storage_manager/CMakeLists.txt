idf_component_register(
    SRCS "storage_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES board_config nvs_flash freertos
)
