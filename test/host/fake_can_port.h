#ifndef FAKE_CAN_PORT_H
#define FAKE_CAN_PORT_H

#include <stdint.h>

#include "can_port.h"

typedef enum {
    FAKE_CAN_LOOPBACK = 0,   /* every sent frame comes back */
    FAKE_CAN_DROP,           /* sent, but never comes back */
    FAKE_CAN_CORRUPT,        /* comes back with data[0] changed */
    FAKE_CAN_TX_FAIL         /* sending fails */
} fake_can_mode_t;

void     fake_can_reset(fake_can_mode_t mode);
void     fake_can_inject(const can_frame_t *frame);   /* put a frame in the RX FIFO */
uint32_t fake_can_sent_count(void);

#endif /* FAKE_CAN_PORT_H */
