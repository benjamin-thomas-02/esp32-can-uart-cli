#include "uart_port.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

#define PORT_UART_NUM   UART_NUM_0   /* UART0 = the USB serial you already use */
#define RX_BUF_SIZE     1024

uart_port_status_t uart_port_init(uint32_t baud)
{
    const uart_config_t cfg = {
        .baud_rate  = (int)baud,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (uart_driver_install(PORT_UART_NUM, RX_BUF_SIZE, 0, 0, NULL, 0) != ESP_OK) {
        return UART_PORT_ERR;
    }
    if (uart_param_config(PORT_UART_NUM, &cfg) != ESP_OK) {
        return UART_PORT_ERR;
    }
    return UART_PORT_OK;
}

int uart_port_read(uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    return uart_read_bytes(PORT_UART_NUM, buf, len, pdMS_TO_TICKS(timeout_ms));
}

int uart_port_write(const uint8_t *buf, size_t len)
{
    return uart_write_bytes(PORT_UART_NUM, buf, len);
}
