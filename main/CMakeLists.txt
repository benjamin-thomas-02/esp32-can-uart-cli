idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES ports platform_esp32 cli can_service esp_timer)
