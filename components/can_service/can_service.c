#include "can_service.h"

#include <string.h>

void can_service_make_test_frame(uint32_t index, can_frame_t *frame)
{
    memset(frame, 0, sizeof(*frame));
    frame->id       = CAN_SELFTEST_BASE_ID + index;
    frame->extended = false;
    frame->dlc      = CAN_MAX_DLC;
    for (uint8_t j = 0; j < CAN_MAX_DLC; j++) {
        frame->data[j] = (uint8_t)(index + j);
    }
}

bool can_service_frames_equal(const can_frame_t *a, const can_frame_t *b)
{
    if (a->id != b->id || a->extended != b->extended || a->dlc != b->dlc) {
        return false;
    }
    return memcmp(a->data, b->data, a->dlc) == 0;
}

bool can_service_selftest(uint32_t num_frames, uint32_t timeout_ms,
                          can_selftest_result_t *result)
{
    can_frame_t tx;
    can_frame_t rx;

    if (result == NULL) {
        return false;
    }
    memset(result, 0, sizeof(*result));

    if (num_frames == 0 || num_frames > CAN_SELFTEST_MAX_FRAMES) {
        return false;
    }

    /* drain old frames so they don't confuse this test */
    while (can_port_receive(&rx, 0) == CAN_PORT_OK) {
    }

    for (uint32_t i = 0; i < num_frames; i++) {
        can_service_make_test_frame(i, &tx);

        if (can_port_send(&tx, timeout_ms) != CAN_PORT_OK) {
            result->tx_errors++;
            continue;
        }
        result->sent++;

        if (can_port_receive(&rx, timeout_ms) != CAN_PORT_OK) {
            result->timeouts++;
            continue;
        }

        if (can_service_frames_equal(&tx, &rx)) {
            result->received_ok++;
        } else {
            result->mismatches++;
        }
    }

    return result->received_ok == num_frames;
}
