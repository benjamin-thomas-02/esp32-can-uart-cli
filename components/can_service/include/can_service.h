#ifndef CAN_SERVICE_H
#define CAN_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "can_port.h"

#define CAN_SELFTEST_BASE_ID     0x100
#define CAN_SELFTEST_MAX_FRAMES  100

typedef struct {
    uint32_t sent;          /* frames sent successfully */
    uint32_t received_ok;   /* frames received back and identical */
    uint32_t mismatches;    /* received, but content differs */
    uint32_t timeouts;      /* sent, but nothing received in time */
    uint32_t tx_errors;     /* could not send */
} can_selftest_result_t;

/* Build the i-th test frame: ID = 0x100 + i, 8 bytes = i, i+1, ... i+7 */
void can_service_make_test_frame(uint32_t index, can_frame_t *frame);

/* true if ID, type, DLC and data bytes are all equal */
bool can_service_frames_equal(const can_frame_t *a, const can_frame_t *b);

/* Send num_frames test frames and check each one comes back.
 * Returns true only if ALL frames came back identical. */
bool can_service_selftest(uint32_t num_frames, uint32_t timeout_ms,
                          can_selftest_result_t *result);

#endif /* CAN_SERVICE_H */
