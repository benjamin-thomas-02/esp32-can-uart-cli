#include "fake_can_port.h"

#include <stddef.h>

#define FAKE_CAN_FIFO_LEN 32

static fake_can_mode_t s_mode;
static can_frame_t     s_fifo[FAKE_CAN_FIFO_LEN];
static size_t          s_head;    /* index of the oldest frame */
static size_t          s_count;   /* number of frames stored */
static uint32_t        s_sent;

static void fifo_push(const can_frame_t *frame)
{
    if (s_count < FAKE_CAN_FIFO_LEN) {
        s_fifo[(s_head + s_count) % FAKE_CAN_FIFO_LEN] = *frame;
        s_count++;
    }
}

void fake_can_reset(fake_can_mode_t mode)
{
    s_mode  = mode;
    s_head  = 0;
    s_count = 0;
    s_sent  = 0;
}

void fake_can_inject(const can_frame_t *frame)
{
    fifo_push(frame);
}

uint32_t fake_can_sent_count(void)
{
    return s_sent;
}

/* ---------- the 3 functions from can_port.h ---------- */
can_port_status_t can_port_init(uint32_t bitrate, can_port_mode_t mode)
{
    (void)bitrate;
    (void)mode;
    return CAN_PORT_OK;
}

can_port_status_t can_port_send(const can_frame_t *frame, uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (frame == NULL || s_mode == FAKE_CAN_TX_FAIL) {
        return CAN_PORT_ERR;
    }
    s_sent++;

    if (s_mode == FAKE_CAN_LOOPBACK) {
        fifo_push(frame);
    } else if (s_mode == FAKE_CAN_CORRUPT) {
        can_frame_t bad = *frame;
        bad.data[0] ^= 0xFF;                 /* flip all bits of byte 0 */
        fifo_push(&bad);
    }
    /* FAKE_CAN_DROP: nothing comes back */
    return CAN_PORT_OK;
}

can_port_status_t can_port_receive(can_frame_t *frame, uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (frame == NULL) {
        return CAN_PORT_ERR;
    }
    if (s_count == 0) {
        return CAN_PORT_TIMEOUT;             /* nothing there = timeout */
    }
    *frame = s_fifo[s_head];
    s_head = (s_head + 1) % FAKE_CAN_FIFO_LEN;
    s_count--;
    return CAN_PORT_OK;
}
