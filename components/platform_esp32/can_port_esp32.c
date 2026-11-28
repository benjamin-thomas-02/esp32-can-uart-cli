#include "can_port.h"

#include <string.h>

#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define CAN_TX_GPIO        21
#define CAN_RX_GPIO        21   /* same pin as TX: internal loopback, no wire needed */
#define CAN_RX_QUEUE_LEN   16
#define CAN_TX_QUEUE_DEPTH 4

static twai_node_handle_t s_node;
static QueueHandle_t      s_rx_queue;

/* TX frame + buffer must stay valid until the hardware has sent them. */
static twai_frame_t s_tx_frame;
static uint8_t      s_tx_buf[CAN_MAX_DLC];

/* Runs in INTERRUPT context when a frame was received. Keep it short! */
static bool on_rx_done(twai_node_handle_t handle,
                       const twai_rx_done_event_data_t *edata,
                       void *user_ctx)
{
    (void)edata;
    (void)user_ctx;

    uint8_t      buf[CAN_MAX_DLC];
    twai_frame_t rx    = { .buffer = buf, .buffer_len = sizeof(buf) };
    BaseType_t   woken = pdFALSE;

    if (twai_node_receive_from_isr(handle, &rx) == ESP_OK) {
        can_frame_t f = { 0 };
        f.id       = rx.header.id;
        f.extended = rx.header.ide;
        f.dlc      = (rx.header.dlc > CAN_MAX_DLC) ? CAN_MAX_DLC : (uint8_t)rx.header.dlc;
        memcpy(f.data, buf, f.dlc);
        xQueueSendFromISR(s_rx_queue, &f, &woken);
    }
    return (woken == pdTRUE);   /* tell the driver if a task must run right away */
}

static void can_port_cleanup(void)
{
    if (s_node != NULL) {
        twai_node_delete(s_node);
        s_node = NULL;
    }
    if (s_rx_queue != NULL) {
        vQueueDelete(s_rx_queue);
        s_rx_queue = NULL;
    }
}

can_port_status_t can_port_init(uint32_t bitrate, can_port_mode_t mode)
{
    if (s_node != NULL) {
        return CAN_PORT_ERR;                        /* already initialised */
    }

    s_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_frame_t));
    if (s_rx_queue == NULL) {
        return CAN_PORT_ERR;
    }

    twai_onchip_node_config_t cfg = {
        .io_cfg = {
            .tx                = CAN_TX_GPIO,
            .rx                = CAN_RX_GPIO,
            .quanta_clk_out    = -1,                /* not used */
            .bus_off_indicator = -1,                /* not used */
        },
        .bit_timing     = { .bitrate = bitrate },
        .tx_queue_depth = CAN_TX_QUEUE_DEPTH,
        .fail_retry_cnt = 0,                        /* single-shot: no endless retries */
    };
    if (mode == CAN_PORT_MODE_SELF_TEST) {
        cfg.flags.enable_self_test = true;          /* don't require ACK */
        cfg.flags.enable_loopback  = true;          /* receive our own frames */
    }

    if (twai_new_node_onchip(&cfg, &s_node) != ESP_OK) {
        s_node = NULL;
        can_port_cleanup();
        return CAN_PORT_ERR;
    }

    twai_event_callbacks_t cbs = { .on_rx_done = on_rx_done };
    if (twai_node_register_event_callbacks(s_node, &cbs, NULL) != ESP_OK ||
        twai_node_enable(s_node) != ESP_OK) {
        can_port_cleanup();
        return CAN_PORT_ERR;
    }
    return CAN_PORT_OK;
}

can_port_status_t can_port_send(const can_frame_t *frame, uint32_t timeout_ms)
{
    if (s_node == NULL || frame == NULL || frame->dlc > CAN_MAX_DLC) {
        return CAN_PORT_ERR;
    }

    memcpy(s_tx_buf, frame->data, frame->dlc);
    memset(&s_tx_frame, 0, sizeof(s_tx_frame));
    s_tx_frame.header.id  = frame->id;
    s_tx_frame.header.ide = frame->extended;
    s_tx_frame.header.dlc = frame->dlc;
    s_tx_frame.buffer     = s_tx_buf;
    s_tx_frame.buffer_len = frame->dlc;

    esp_err_t err = twai_node_transmit(s_node, &s_tx_frame, (int)timeout_ms);
    if (err == ESP_OK) {
        /* wait until really sent, so s_tx_frame can be reused safely */
        err = twai_node_transmit_wait_all_done(s_node, (int)timeout_ms);
    }

    if (err == ESP_ERR_TIMEOUT) {
        return CAN_PORT_TIMEOUT;
    }
    return (err == ESP_OK) ? CAN_PORT_OK : CAN_PORT_ERR;
}

can_port_status_t can_port_receive(can_frame_t *frame, uint32_t timeout_ms)
{
    if (s_rx_queue == NULL || frame == NULL) {
        return CAN_PORT_ERR;
    }
    if (xQueueReceive(s_rx_queue, frame, pdMS_TO_TICKS(timeout_ms)) != pdPASS) {
        return CAN_PORT_TIMEOUT;
    }
    return CAN_PORT_OK;
}
