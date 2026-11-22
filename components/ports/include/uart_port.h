#ifndef UART_PORT_H
#define UART_PORT_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    UART_PORT_OK  = 0,
    UART_PORT_ERR = -1
} uart_port_status_t;

/* Initialise the UART with the given baud rate. */
uart_port_status_t uart_port_init(uint32_t baud);

/* Read up to len bytes, waiting at most timeout_ms.
 * Returns number of bytes read (0 on timeout), or -1 on error. */
int uart_port_read(uint8_t *buf, size_t len, uint32_t timeout_ms);

/* Write len bytes. Returns number of bytes written, or -1 on error. */
int uart_port_write(const uint8_t *buf, size_t len);

#endif /* UART_PORT_H */
