#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "uart_port.h"
#include "cli.h"

static const char *TAG = "main";

#define APP_VERSION          "0.1.0"
#define UART_BAUD            115200
#define PROMPT               "> "

#define CLI_QUEUE_LEN        4
#define UART_RX_TASK_STACK   4096
#define UART_RX_TASK_PRIO    5
#define CLI_TASK_STACK       4096
#define CLI_TASK_PRIO        4

/* Message passed through the queue: one complete command line. */
typedef struct {
    char text[CLI_MAX_LINE];
} cli_msg_t;

static QueueHandle_t s_cli_queue;

/* ---------- output function given to the CLI ---------- */
static void console_write(const char *text)
{
    uart_port_write((const uint8_t *)text, strlen(text));
}

/* ---------- app commands ---------- */
static cli_status_t cmd_version(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_print("esp32-can-uart-cli v" APP_VERSION "\r\n");
    return CLI_OK;
}

static cli_status_t cmd_uptime(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    char out[48];
    int64_t ms = esp_timer_get_time() / 1000;   /* esp_timer gives microseconds */
    snprintf(out, sizeof(out), "uptime: %" PRId64 " ms\r\n", ms);
    cli_print(out);
    return CLI_OK;
}

static cli_status_t cmd_echo(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {            /* argv[0] is "echo" itself */
        cli_print(argv[i]);
        if (i < argc - 1) {
            cli_print(" ");
        }
    }
    cli_print("\r\n");
    return CLI_OK;
}

static const cli_command_t s_app_commands[] = {
    { .name = "version", .help = "Show firmware version",   .handler = cmd_version },
    { .name = "uptime",  .help = "Time since boot in ms",   .handler = cmd_uptime  },
    { .name = "echo",    .help = "Print the arguments back", .handler = cmd_echo    },
};

/* ---------- task 1: receive characters, build lines ---------- */
static void uart_rx_task(void *arg)
{
    (void)arg;
    cli_line_t line;
    uint8_t    ch;

    cli_line_init(&line);

    for (;;) {
        if (uart_port_read(&ch, 1, 100) != 1) {
            continue;                                   /* timeout */
        }

        size_t before = line.len;
        cli_line_result_t res = cli_line_feed(&line, (char)ch);

        if (res == CLI_LINE_READY) {
            cli_msg_t msg;
            console_write("\r\n");
            memcpy(msg.text, line.buf, sizeof(msg.text));
            if (xQueueSend(s_cli_queue, &msg, 0) != pdPASS) {
                console_write("ERR: busy, command dropped\r\n");
            }
            cli_line_clear(&line);
        } else if (line.len > before) {
            uart_port_write(&ch, 1);                    /* char added: echo it */
        } else if (line.len < before) {
            console_write("\b \b");                     /* char removed: erase on screen */
        }
    }
}

/* ---------- task 2: run commands ---------- */
static void cli_task(void *arg)
{
    (void)arg;
    cli_msg_t msg;

    console_write(PROMPT);

    for (;;) {
        if (xQueueReceive(s_cli_queue, &msg, portMAX_DELAY) == pdPASS) {
            cli_status_t st = cli_execute(msg.text);
            if (st != CLI_OK && st != CLI_ERR_EMPTY) {
                char out[64];
                snprintf(out, sizeof(out), "ERR: %s\r\n", cli_status_str(st));
                console_write(out);
            }
            console_write(PROMPT);
        }
    }
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(50));      /* let the boot log finish */

    if (uart_port_init(UART_BAUD) != UART_PORT_OK) {
        ESP_LOGE(TAG, "UART init failed");
        return;
    }

    cli_init(console_write);
    for (size_t i = 0; i < sizeof(s_app_commands) / sizeof(s_app_commands[0]); i++) {
        if (cli_register(&s_app_commands[i]) != CLI_OK) {
            ESP_LOGE(TAG, "Failed to register '%s'", s_app_commands[i].name);
        }
    }

    s_cli_queue = xQueueCreate(CLI_QUEUE_LEN, sizeof(cli_msg_t));
    if (s_cli_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create CLI queue");
        return;
    }

    ESP_LOGI(TAG, "esp32-can-uart-cli v%s - type 'help'", APP_VERSION);

    if (xTaskCreate(cli_task, "cli", CLI_TASK_STACK, NULL, CLI_TASK_PRIO, NULL) != pdPASS ||
        xTaskCreate(uart_rx_task, "uart_rx", UART_RX_TASK_STACK, NULL, UART_RX_TASK_PRIO, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create tasks");
    }
}
