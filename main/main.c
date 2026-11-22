#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "uart_port.h"

static const char *TAG = "main";

#define UART_BAUD           115200
#define UART_RX_TASK_STACK  4096
#define UART_RX_TASK_PRIO   5

/* Task: read one character at a time and echo it back. */
static void uart_rx_task(void *arg)
{
    (void)arg;
    uint8_t ch;

    for (;;) {
        int n = uart_port_read(&ch, 1, 100);
        if (n != 1) {
            continue;                       /* timeout: nothing typed */
        }

        if (ch == '\r') {
            uart_port_write((const uint8_t *)"\r\n", 2);   /* Enter -> new line */
        } else if (ch == '\n') {
            /* ignore: some terminals send \r\n, we already handled \r */
        } else {
            uart_port_write(&ch, 1);        /* echo the character */
        }
    }
}

void app_main(void)
{
    if (uart_port_init(UART_BAUD) != UART_PORT_OK) {
        ESP_LOGE(TAG, "UART init failed");
        return;
    }

    ESP_LOGI(TAG, "UART echo ready - type something:");

    BaseType_t ok = xTaskCreate(uart_rx_task, "uart_rx",
                                UART_RX_TASK_STACK, NULL,
                                UART_RX_TASK_PRIO, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create uart_rx task");
    }
}
