#ifndef CAN_PORT_H
#define CAN_PORT_H

#include <stdbool.h>
#include <stdint.h>

#define CAN_MAX_DLC  8   /* classic CAN: max 8 data bytes */

/* Hardware-independent CAN frame. */
typedef struct {
    uint32_t id;                  /* 11-bit (standard) or 29-bit (extended) ID */
    bool     extended;            /* true = 29-bit ID */
    uint8_t  dlc;                 /* number of data bytes, 0..8 */
    uint8_t  data[CAN_MAX_DLC];
} can_frame_t;

typedef enum {
    CAN_PORT_OK      = 0,
    CAN_PORT_ERR     = -1,
    CAN_PORT_TIMEOUT = -2
} can_port_status_t;

typedef enum {
    CAN_PORT_MODE_NORMAL = 0,     /* real bus: needs transceiver + other nodes */
    CAN_PORT_MODE_SELF_TEST       /* single node: no ACK needed, receive own frames */
} can_port_mode_t;

can_port_status_t can_port_init(uint32_t bitrate, can_port_mode_t mode);
can_port_status_t can_port_send(const can_frame_t *frame, uint32_t timeout_ms);
can_port_status_t can_port_receive(can_frame_t *frame, uint32_t timeout_ms);

#endif /* CAN_PORT_H */
