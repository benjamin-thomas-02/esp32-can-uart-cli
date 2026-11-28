#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "uart_port.h"
#include "can_port.h"
#include "can_service.h"
#include "cli.h"

static const char *TAG = "main";

#define APP_VERSION          "0.3.0"
#define UART_BAUD            115200
#define PROMPT               "> "

#define CAN_BITRATE          500000
#define CAN_TIMEOUT_MS       100
#define CAN_SELFTEST_DEFAULT 10
#define CAN_RX_POLL_MS       50      /* max time can_rx_task holds the RX lock */
#define CAN_LOCK_TIMEOUT_MS  500

#define CLI_QUEUE_LEN        4
#define UART_RX_TASK_STACK   4096
#define UART_RX_TASK_PRIO    5
#define CLI_TASK_STACK       4096
#define CLI_TASK_PRIO        4
#define CAN_RX_TASK_STACK    4096
#define CAN_RX_TASK_PRIO     3

typedef struct {
    char text[CLI_MAX_LINE];
} cli_msg_t;

static QueueHandle_t     s_cli_queue;
static SemaphoreHandle_t s_console_mutex;   /* protects UART output */
static SemaphoreHandle_t s_can_rx_lock;     /* who may call can_port_receive() */
static bool              s_can_ready;
static volatile bool     s_can_monitor;     /* written by cli_task, read by can_rx_task */

static TaskHandle_t s_uart_rx_task_handle;
static TaskHandle_t s_cli_task_handle;
static TaskHandle_t s_can_rx_task_handle;

/* ================= console output (mutex-protected) ================= */
static void console_write_bytes(const uint8_t *data, size_t len)
{
    if (s_console_mutex != NULL) {
        xSemaphoreTake(s_console_mutex, portMAX_DELAY);
    }
    uart_port_write(data, len);
    if (s_console_mutex != NULL) {
        xSemaphoreGive(s_console_mutex);
    }
}

static void console_write(const char *text)
{
    console_write_bytes((const uint8_t *)text, strlen(text));
}

/* ================= basic commands ================= */
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
    int64_t ms = esp_timer_get_time() / 1000;
    snprintf(out, sizeof(out), "uptime: %" PRId64 " ms\r\n", ms);
    cli_print(out);
    return CLI_OK;
}

static cli_status_t cmd_echo(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        cli_print(argv[i]);
        if (i < argc - 1) {
            cli_print(" ");
        }
    }
    cli_print("\r\n");
    return CLI_OK;
}

static void print_task_stack(const char *name, TaskHandle_t handle)
{
    char out[64];
    if (handle == NULL) {
        return;
    }
    /* ESP-IDF: high water mark is in BYTES (lowest free stack ever seen) */
    snprintf(out, sizeof(out), "  %-8s stack free (min): %u bytes\r\n",
             name, (unsigned)uxTaskGetStackHighWaterMark(handle));
    cli_print(out);
}

static cli_status_t cmd_sys(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    char out[96];

    snprintf(out, sizeof(out), "heap free: %" PRIu32 " bytes, min ever: %" PRIu32 " bytes\r\n",
             esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
    cli_print(out);
    print_task_stack("uart_rx", s_uart_rx_task_handle);
    print_task_stack("cli",     s_cli_task_handle);
    print_task_stack("can_rx",  s_can_rx_task_handle);
    return CLI_OK;
}

/* ================= CAN subcommands ================= */
static cli_status_t can_cmd_send(int argc, char *argv[])
{
    can_frame_t frame;
    char        frame_txt[64];
    char        out[96];

    /* argv[0]="can", argv[1]="send", argv[2..]=id + bytes */
    can_parse_status_t ps = can_service_parse_frame(argc - 2, &argv[2], &frame);
    if (ps != CAN_PARSE_OK) {
        snprintf(out, sizeof(out), "%s\r\nusage: can send <id> [b0 .. b7]  (hex)\r\n",
                 can_service_parse_status_str(ps));
        cli_print(out);
        return CLI_ERR_INVALID_ARG;
    }

    can_service_format_frame(&frame, frame_txt, sizeof(frame_txt));
    if (can_service_send(&frame, CAN_TIMEOUT_MS) != CAN_PORT_OK) {
        snprintf(out, sizeof(out), "TX failed: %s\r\n", frame_txt);
    } else {
        snprintf(out, sizeof(out), "TX %s\r\n", frame_txt);
    }
    cli_print(out);
    return CLI_OK;
}

static cli_status_t can_cmd_stats(int argc, char *argv[])
{
    char        out[96];
    can_stats_t s;

    if (argc >= 3 && strcmp(argv[2], "reset") == 0) {
        can_service_stats_reset();
        cli_print("CAN stats reset\r\n");
        return CLI_OK;
    }

    can_service_get_stats(&s);
    snprintf(out, sizeof(out), "tx_ok=%" PRIu32 " tx_err=%" PRIu32 " rx=%" PRIu32 "\r\n",
             s.tx_ok, s.tx_err, s.rx);
    cli_print(out);
    return CLI_OK;
}

static cli_status_t can_cmd_monitor(int argc, char *argv[])
{
    if (argc >= 3 && strcmp(argv[2], "on") == 0) {
        s_can_monitor = true;
        cli_print("CAN monitor on\r\n");
    } else if (argc >= 3 && strcmp(argv[2], "off") == 0) {
        s_can_monitor = false;
        cli_print("CAN monitor off\r\n");
    } else {
        cli_print(s_can_monitor ? "CAN monitor is on\r\n" : "CAN monitor is off\r\n");
    }
    return CLI_OK;
}

static cli_status_t can_cmd_selftest(int argc, char *argv[])
{
    uint32_t frames = CAN_SELFTEST_DEFAULT;

    if (argc >= 3) {
        char *end = NULL;
        unsigned long value = strtoul(argv[2], &end, 10);
        if (*end != '\0' || value < 1 || value > CAN_SELFTEST_MAX_FRAMES) {
            cli_print("frames must be a number 1-100\r\n");
            return CLI_ERR_INVALID_ARG;
        }
        frames = (uint32_t)value;
    }

    /* take ownership of the receiver so can_rx_task doesn't steal our frames */
    if (xSemaphoreTake(s_can_rx_lock, pdMS_TO_TICKS(CAN_LOCK_TIMEOUT_MS)) != pdTRUE) {
        cli_print("CAN busy, try again\r\n");
        return CLI_ERR_INVALID_ARG;
    }
    can_selftest_result_t res;
    bool pass = can_service_selftest(frames, CAN_TIMEOUT_MS, &res);
    xSemaphoreGive(s_can_rx_lock);                 /* give back BEFORE printing */

    char out[128];
    snprintf(out, sizeof(out),
             "CAN self-test %s: sent=%" PRIu32 " ok=%" PRIu32 " mismatch=%" PRIu32
             " timeout=%" PRIu32 " tx_err=%" PRIu32 "\r\n",
             pass ? "PASS" : "FAIL",
             res.sent, res.received_ok, res.mismatches, res.timeouts, res.tx_errors);
    cli_print(out);
    return CLI_OK;
}

typedef struct {
    const char   *name;
    cli_handler_t handler;
} can_subcmd_t;

static const can_subcmd_t s_can_subcmds[] = {
    { "send",     can_cmd_send     },
    { "stats",    can_cmd_stats    },
    { "monitor",  can_cmd_monitor  },
    { "selftest", can_cmd_selftest },
};

static void can_print_usage(void)
{
    cli_print("usage:\r\n"
              "  can send <id> [b0 .. b7]   (hex)\r\n"
              "  can stats [reset]\r\n"
              "  can monitor [on|off]\r\n"
              "  can selftest [frames 1-100]\r\n");
}

static cli_status_t cmd_can(int argc, char *argv[])
{
    if (!s_can_ready) {
        cli_print("CAN not initialised\r\n");
        return CLI_ERR_INVALID_ARG;
    }
    if (argc >= 2) {
        for (size_t i = 0; i < sizeof(s_can_subcmds) / sizeof(s_can_subcmds[0]); i++) {
            if (strcmp(argv[1], s_can_subcmds[i].name) == 0) {
                return s_can_subcmds[i].handler(argc, argv);
            }
        }
    }
    can_print_usage();
    return CLI_ERR_INVALID_ARG;
}

static const cli_command_t s_app_commands[] = {
    { .name = "version", .help = "Show firmware version",              .handler = cmd_version },
    { .name = "uptime",  .help = "Time since boot in ms",              .handler = cmd_uptime  },
    { .name = "echo",    .help = "Print the arguments back",           .handler = cmd_echo    },
    { .name = "sys",     .help = "Heap and task stack usage",          .handler = cmd_sys     },
    { .name = "can",     .help = "CAN: send | stats | monitor | selftest", .handler = cmd_can },
};

/* ================= task 1: UART input ================= */
static void uart_rx_task(void *arg)
{
    (void)arg;
    cli_line_t line;
    uint8_t    ch;

    cli_line_init(&line);

    for (;;) {
        if (uart_port_read(&ch, 1, 100) != 1) {
            continue;
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
            console_write_bytes(&ch, 1);           /* echo, now mutex-protected */
        } else if (line.len < before) {
            console_write("\b \b");
        }
    }
}

/* ================= task 2: run commands ================= */
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

/* ================= task 3: CAN receive ================= */
static void can_rx_task(void *arg)
{
    (void)arg;
    can_frame_t frame;
    char        txt[64];
    char        line[80];

    for (;;) {
        xSemaphoreTake(s_can_rx_lock, portMAX_DELAY);
        can_port_status_t st = can_port_receive(&frame, CAN_RX_POLL_MS);
        xSemaphoreGive(s_can_rx_lock);             /* never hold it while printing */

        if (st != CAN_PORT_OK) {
            continue;                              /* timeout: nothing received */
        }

        can_service_on_rx(&frame);
        if (s_can_monitor) {
            can_service_format_frame(&frame, txt, sizeof(txt));
            snprintf(line, sizeof(line), "[CAN RX] %s\r\n", txt);
            console_write(line);
        }
    }
}

/* ================= startup ================= */
void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(50));

    if (uart_port_init(UART_BAUD) != UART_PORT_OK) {
        ESP_LOGE(TAG, "UART init failed");
        return;
    }

    s_console_mutex = xSemaphoreCreateMutex();
    s_can_rx_lock   = xSemaphoreCreateMutex();
    s_cli_queue     = xQueueCreate(CLI_QUEUE_LEN, sizeof(cli_msg_t));
    if (s_console_mutex == NULL || s_can_rx_lock == NULL || s_cli_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create RTOS objects");
        return;
    }

    if (can_port_init(CAN_BITRATE, CAN_PORT_MODE_SELF_TEST) == CAN_PORT_OK) {
        s_can_ready = true;
        ESP_LOGI(TAG, "CAN ready (self-test mode, %d kbit/s)", CAN_BITRATE / 1000);
    } else {
        ESP_LOGE(TAG, "CAN init failed - 'can' commands disabled");
    }

    cli_init(console_write);
    for (size_t i = 0; i < sizeof(s_app_commands) / sizeof(s_app_commands[0]); i++) {
        if (cli_register(&s_app_commands[i]) != CLI_OK) {
            ESP_LOGE(TAG, "Failed to register '%s'", s_app_commands[i].name);
        }
    }

    ESP_LOGI(TAG, "esp32-can-uart-cli v%s - type 'help'", APP_VERSION);

    if (xTaskCreate(cli_task, "cli", CLI_TASK_STACK, NULL, CLI_TASK_PRIO,
                    &s_cli_task_handle) != pdPASS ||
        xTaskCreate(uart_rx_task, "uart_rx", UART_RX_TASK_STACK, NULL, UART_RX_TASK_PRIO,
                    &s_uart_rx_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create tasks");
        return;
    }

    if (s_can_ready &&
        xTaskCreate(can_rx_task, "can_rx", CAN_RX_TASK_STACK, NULL, CAN_RX_TASK_PRIO,
                    &s_can_rx_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create can_rx task");
    }
}
