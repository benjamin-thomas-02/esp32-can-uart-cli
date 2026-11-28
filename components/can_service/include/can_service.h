#ifndef CAN_SERVICE_H
#define CAN_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "can_port.h"

#define CAN_SELFTEST_BASE_ID     0x100
#define CAN_SELFTEST_MAX_FRAMES  100

#define CAN_STD_ID_MAX           0x7FFu        /* 11-bit standard ID */
#define CAN_EXT_ID_MAX           0x1FFFFFFFu   /* 29-bit extended ID */

/* ================= self-test (from M5) ================= */
typedef struct {
    uint32_t sent;          /* frames sent successfully */
    uint32_t received_ok;   /* frames received back and identical */
    uint32_t mismatches;    /* received, but content differs */
    uint32_t timeouts;      /* sent, but nothing received in time */
    uint32_t tx_errors;     /* could not send */
} can_selftest_result_t;

void can_service_make_test_frame(uint32_t index, can_frame_t *frame);
bool can_service_frames_equal(const can_frame_t *a, const can_frame_t *b);
bool can_service_selftest(uint32_t num_frames, uint32_t timeout_ms,
                          can_selftest_result_t *result);

/* ================= parsing "can send <id> [b0 .. b7]" (NEW) ================= */
typedef enum {
    CAN_PARSE_OK = 0,
    CAN_PARSE_ERR_MISSING_ID,     /* no ID given */
    CAN_PARSE_ERR_BAD_ID,         /* ID is not a hex number */
    CAN_PARSE_ERR_ID_RANGE,       /* ID > 0x1FFFFFFF */
    CAN_PARSE_ERR_BAD_BYTE,       /* data byte not hex, or > 0xFF */
    CAN_PARSE_ERR_TOO_MANY_BYTES  /* more than 8 data bytes */
} can_parse_status_t;

/* args[0] = ID (hex), args[1..] = data bytes (hex). IDs > 0x7FF become extended. */
can_parse_status_t can_service_parse_frame(int argc, char *args[], can_frame_t *frame);
const char        *can_service_parse_status_str(can_parse_status_t status);

/* ================= formatting (NEW) ================= */
/* e.g. "ID=0x123 DLC=2 11 22"  or  "ID=0x00012345 EXT DLC=0" */
void can_service_format_frame(const can_frame_t *frame, char *buf, size_t size);

/* ================= statistics (NEW) ================= */
typedef struct {
    uint32_t tx_ok;
    uint32_t tx_err;
    uint32_t rx;
} can_stats_t;

void              can_service_stats_reset(void);
can_port_status_t can_service_send(const can_frame_t *frame, uint32_t timeout_ms);
void              can_service_on_rx(const can_frame_t *frame);
void              can_service_get_stats(can_stats_t *out);

#endif /* CAN_SERVICE_H */
