#include "can_service.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* ================= self-test (from M5) ================= */
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

    while (can_port_receive(&rx, 0) == CAN_PORT_OK) {
        /* drain old frames */
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

/* ================= parsing (NEW) ================= */

/* Parse hex text like "1A", "0x1a", "0X7FF" into a number.
 * Returns false if empty, not hex, or more than 8 hex digits. */
static bool parse_hex_u32(const char *s, uint32_t *out)
{
    uint32_t value  = 0;
    int      digits = 0;

    if (s == NULL) {
        return false;
    }
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;                                  /* skip optional "0x" */
    }
    if (*s == '\0') {
        return false;                            /* nothing after prefix */
    }

    for (; *s != '\0'; s++) {
        uint32_t d;
        if (*s >= '0' && *s <= '9') {
            d = (uint32_t)(*s - '0');
        } else if (*s >= 'a' && *s <= 'f') {
            d = (uint32_t)(*s - 'a' + 10);
        } else if (*s >= 'A' && *s <= 'F') {
            d = (uint32_t)(*s - 'A' + 10);
        } else {
            return false;                        /* not a hex digit */
        }
        if (++digits > 8) {
            return false;                        /* would overflow 32 bits */
        }
        value = (value << 4) | d;                /* shift in 4 bits per hex digit */
    }

    *out = value;
    return true;
}

can_parse_status_t can_service_parse_frame(int argc, char *args[], can_frame_t *frame)
{
    uint32_t value;

    if (frame == NULL || args == NULL || argc < 1) {
        return CAN_PARSE_ERR_MISSING_ID;
    }
    memset(frame, 0, sizeof(*frame));

    /* --- ID --- */
    if (!parse_hex_u32(args[0], &value)) {
        return CAN_PARSE_ERR_BAD_ID;
    }
    if (value > CAN_EXT_ID_MAX) {
        return CAN_PARSE_ERR_ID_RANGE;
    }
    frame->id       = value;
    frame->extended = (value > CAN_STD_ID_MAX);

    /* --- data bytes --- */
    int num_bytes = argc - 1;
    if (num_bytes > CAN_MAX_DLC) {
        return CAN_PARSE_ERR_TOO_MANY_BYTES;
    }
    for (int i = 0; i < num_bytes; i++) {
        if (!parse_hex_u32(args[i + 1], &value) || value > 0xFFu) {
            return CAN_PARSE_ERR_BAD_BYTE;
        }
        frame->data[i] = (uint8_t)value;
    }
    frame->dlc = (uint8_t)num_bytes;

    return CAN_PARSE_OK;
}

const char *can_service_parse_status_str(can_parse_status_t status)
{
    switch (status) {
    case CAN_PARSE_OK:                 return "ok";
    case CAN_PARSE_ERR_MISSING_ID:     return "missing CAN ID";
    case CAN_PARSE_ERR_BAD_ID:         return "CAN ID must be hex, e.g. 0x123";
    case CAN_PARSE_ERR_ID_RANGE:       return "CAN ID too large (max 0x1FFFFFFF)";
    case CAN_PARSE_ERR_BAD_BYTE:       return "data byte must be hex 00-FF";
    case CAN_PARSE_ERR_TOO_MANY_BYTES: return "max 8 data bytes";
    default:                           return "unknown parse error";
    }
}

/* ================= formatting (NEW) ================= */
void can_service_format_frame(const can_frame_t *frame, char *buf, size_t size)
{
    int    n;
    size_t pos;

    if (buf == NULL || size == 0) {
        return;
    }
    buf[0] = '\0';
    if (frame == NULL) {
        return;
    }

    if (frame->extended) {
        n = snprintf(buf, size, "ID=0x%08" PRIX32 " EXT DLC=%u", frame->id, (unsigned)frame->dlc);
    } else {
        n = snprintf(buf, size, "ID=0x%03" PRIX32 " DLC=%u", frame->id, (unsigned)frame->dlc);
    }
    pos = (n < 0) ? 0 : (size_t)n;

    for (uint8_t i = 0; i < frame->dlc && i < CAN_MAX_DLC && pos < size; i++) {
        n = snprintf(&buf[pos], size - pos, " %02X", frame->data[i]);
        if (n < 0) {
            break;
        }
        pos += (size_t)n;
    }
}

/* ================= statistics (NEW) ================= */
static can_stats_t s_stats;

void can_service_stats_reset(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
}

can_port_status_t can_service_send(const can_frame_t *frame, uint32_t timeout_ms)
{
    can_port_status_t st = can_port_send(frame, timeout_ms);
    if (st == CAN_PORT_OK) {
        s_stats.tx_ok++;
    } else {
        s_stats.tx_err++;
    }
    return st;
}

void can_service_on_rx(const can_frame_t *frame)
{
    (void)frame;
    s_stats.rx++;
}

void can_service_get_stats(can_stats_t *out)
{
    if (out != NULL) {
        *out = s_stats;      /* struct copy */
    }
}
