idf_component_register(SRCS "uart_port_esp32.c"
                       REQUIRES ports
                       PRIV_REQUIRES esp_driver_uart)
