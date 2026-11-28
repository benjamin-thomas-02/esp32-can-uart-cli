#include <string.h>

#include "unity.h"
#include "can_service.h"
#include "fake_can_port.h"

void setUp(void)
{
    fake_can_reset(FAKE_CAN_LOOPBACK);
    can_service_stats_reset();
}

void tearDown(void)
{
}

/* ================= test frames ================= */
static void test_make_test_frame_pattern(void)
{
    can_frame_t f;

    can_service_make_test_frame(5, &f);

    TEST_ASSERT_EQUAL_HEX32(0x105, f.id);
    TEST_ASSERT_FALSE(f.extended);
    TEST_ASSERT_EQUAL_UINT8(8, f.dlc);
    TEST_ASSERT_EQUAL_HEX8(0x05, f.data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x0C, f.data[7]);
}

static void test_frames_equal_identical(void)
{
    can_frame_t a;
    can_frame_t b;
    can_service_make_test_frame(1, &a);
    can_service_make_test_frame(1, &b);

    TEST_ASSERT_TRUE(can_service_frames_equal(&a, &b));
}

static void test_frames_equal_detects_id_difference(void)
{
    can_frame_t a;
    can_frame_t b;
    can_service_make_test_frame(1, &a);
    b = a;
    b.id = 0x200;

    TEST_ASSERT_FALSE(can_service_frames_equal(&a, &b));
}

static void test_frames_equal_detects_data_difference(void)
{
    can_frame_t a;
    can_frame_t b;
    can_service_make_test_frame(1, &a);
    b = a;
    b.data[3] ^= 0x01;

    TEST_ASSERT_FALSE(can_service_frames_equal(&a, &b));
}

static void test_frames_equal_ignores_bytes_beyond_dlc(void)
{
    can_frame_t a = { .id = 0x10, .dlc = 2, .data = { 1, 2, 3 } };
    can_frame_t b = { .id = 0x10, .dlc = 2, .data = { 1, 2, 9 } };   /* byte 2 unused */

    TEST_ASSERT_TRUE(can_service_frames_equal(&a, &b));
}

/* ================= self-test with fake port ================= */
static void test_selftest_passes_with_loopback(void)
{
    can_selftest_result_t r;

    TEST_ASSERT_TRUE(can_service_selftest(10, 100, &r));
    TEST_ASSERT_EQUAL_UINT32(10, r.sent);
    TEST_ASSERT_EQUAL_UINT32(10, r.received_ok);
    TEST_ASSERT_EQUAL_UINT32(0, r.timeouts);
}

static void test_selftest_counts_timeouts_when_frames_dropped(void)
{
    can_selftest_result_t r;
    fake_can_reset(FAKE_CAN_DROP);

    TEST_ASSERT_FALSE(can_service_selftest(5, 100, &r));
    TEST_ASSERT_EQUAL_UINT32(5, r.sent);
    TEST_ASSERT_EQUAL_UINT32(5, r.timeouts);
    TEST_ASSERT_EQUAL_UINT32(0, r.received_ok);
}

static void test_selftest_counts_mismatches_when_corrupted(void)
{
    can_selftest_result_t r;
    fake_can_reset(FAKE_CAN_CORRUPT);

    TEST_ASSERT_FALSE(can_service_selftest(5, 100, &r));
    TEST_ASSERT_EQUAL_UINT32(5, r.mismatches);
}

static void test_selftest_counts_tx_errors(void)
{
    can_selftest_result_t r;
    fake_can_reset(FAKE_CAN_TX_FAIL);

    TEST_ASSERT_FALSE(can_service_selftest(5, 100, &r));
    TEST_ASSERT_EQUAL_UINT32(5, r.tx_errors);
    TEST_ASSERT_EQUAL_UINT32(0, r.sent);
}

static void test_selftest_drains_old_frames_first(void)
{
    can_selftest_result_t r;
    can_frame_t stale = { .id = 0x7AA, .dlc = 1, .data = { 0xEE } };
    fake_can_inject(&stale);                         /* leftover from earlier */

    TEST_ASSERT_TRUE(can_service_selftest(3, 100, &r));
    TEST_ASSERT_EQUAL_UINT32(0, r.mismatches);
}

static void test_selftest_rejects_invalid_frame_counts(void)
{
    can_selftest_result_t r;

    TEST_ASSERT_FALSE(can_service_selftest(0, 100, &r));
    TEST_ASSERT_FALSE(can_service_selftest(CAN_SELFTEST_MAX_FRAMES + 1, 100, &r));
    TEST_ASSERT_FALSE(can_service_selftest(10, 100, NULL));
    TEST_ASSERT_EQUAL_UINT32(0, fake_can_sent_count());   /* nothing was sent */
}

/* ================= parsing ================= */
static void test_parse_standard_id_without_data(void)
{
    char       *args[] = { "123" };
    can_frame_t f;

    TEST_ASSERT_EQUAL_INT(CAN_PARSE_OK, can_service_parse_frame(1, args, &f));
    TEST_ASSERT_EQUAL_HEX32(0x123, f.id);
    TEST_ASSERT_FALSE(f.extended);
    TEST_ASSERT_EQUAL_UINT8(0, f.dlc);
}

static void test_parse_id_with_0x_prefix_and_data(void)
{
    char       *args[] = { "0x7FF", "11", "aB" };
    can_frame_t f;

    TEST_ASSERT_EQUAL_INT(CAN_PARSE_OK, can_service_parse_frame(3, args, &f));
    TEST_ASSERT_EQUAL_HEX32(0x7FF, f.id);
    TEST_ASSERT_FALSE(f.extended);
    TEST_ASSERT_EQUAL_UINT8(2, f.dlc);
    TEST_ASSERT_EQUAL_HEX8(0x11, f.data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xAB, f.data[1]);
}

static void test_parse_id_above_7ff_is_extended(void)
{
    char       *args[] = { "0x800" };
    can_frame_t f;

    TEST_ASSERT_EQUAL_INT(CAN_PARSE_OK, can_service_parse_frame(1, args, &f));
    TEST_ASSERT_TRUE(f.extended);
}

static void test_parse_max_extended_id(void)
{
    char       *ok[]  = { "0x1FFFFFFF" };
    char       *bad[] = { "0x20000000" };
    can_frame_t f;

    TEST_ASSERT_EQUAL_INT(CAN_PARSE_OK,           can_service_parse_frame(1, ok, &f));
    TEST_ASSERT_EQUAL_INT(CAN_PARSE_ERR_ID_RANGE, can_service_parse_frame(1, bad, &f));
}

static void test_parse_missing_id(void)
{
    can_frame_t f;

    TEST_ASSERT_EQUAL_INT(CAN_PARSE_ERR_MISSING_ID, can_service_parse_frame(0, NULL, &f));
}

static void test_parse_bad_ids(void)
{
    char       *not_hex[]     = { "xyz" };
    char       *only_prefix[] = { "0x" };
    char       *too_long[]    = { "123456789" };
    can_frame_t f;

    TEST_ASSERT_EQUAL_INT(CAN_PARSE_ERR_BAD_ID, can_service_parse_frame(1, not_hex, &f));
    TEST_ASSERT_EQUAL_INT(CAN_PARSE_ERR_BAD_ID, can_service_parse_frame(1, only_prefix, &f));
    TEST_ASSERT_EQUAL_INT(CAN_PARSE_ERR_BAD_ID, can_service_parse_frame(1, too_long, &f));
}

static void test_parse_bad_bytes(void)
{
    char       *too_big[] = { "100", "1FF" };
    char       *not_hex[] = { "100", "zz" };
    can_frame_t f;

    TEST_ASSERT_EQUAL_INT(CAN_PARSE_ERR_BAD_BYTE, can_service_parse_frame(2, too_big, &f));
    TEST_ASSERT_EQUAL_INT(CAN_PARSE_ERR_BAD_BYTE, can_service_parse_frame(2, not_hex, &f));
}

static void test_parse_eight_bytes_ok_nine_rejected(void)
{
    char       *eight[] = { "100", "1", "2", "3", "4", "5", "6", "7", "8" };
    char       *nine[]  = { "100", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
    can_frame_t f;

    TEST_ASSERT_EQUAL_INT(CAN_PARSE_OK,                 can_service_parse_frame(9, eight, &f));
    TEST_ASSERT_EQUAL_UINT8(8, f.dlc);
    TEST_ASSERT_EQUAL_INT(CAN_PARSE_ERR_TOO_MANY_BYTES, can_service_parse_frame(10, nine, &f));
}

/* ================= formatting ================= */
static void test_format_standard_frame(void)
{
    can_frame_t f = { .id = 0x123, .dlc = 2, .data = { 0x11, 0x22 } };
    char        buf[64];

    can_service_format_frame(&f, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_STRING("ID=0x123 DLC=2 11 22", buf);
}

static void test_format_extended_frame_without_data(void)
{
    can_frame_t f = { .id = 0x12345, .extended = true, .dlc = 0 };
    char        buf[64];

    can_service_format_frame(&f, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_STRING("ID=0x00012345 EXT DLC=0", buf);
}

static void test_format_small_buffer_is_truncated_safely(void)
{
    can_frame_t f = { .id = 0x123, .dlc = 8, .data = { 1, 2, 3, 4, 5, 6, 7, 8 } };
    char        buf[10];

    memset(buf, 'Z', sizeof(buf));
    can_service_format_frame(&f, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_UINT(9, strlen(buf));          /* 9 chars + '\0' = 10 bytes */
    TEST_ASSERT_EQUAL_STRING("ID=0x123 ", buf);
}

/* ================= statistics ================= */
static void test_stats_start_at_zero(void)
{
    can_stats_t s;

    can_service_get_stats(&s);

    TEST_ASSERT_EQUAL_UINT32(0, s.tx_ok);
    TEST_ASSERT_EQUAL_UINT32(0, s.tx_err);
    TEST_ASSERT_EQUAL_UINT32(0, s.rx);
}

static void test_stats_count_tx_ok_and_rx(void)
{
    can_frame_t f = { .id = 0x10, .dlc = 0 };
    can_stats_t s;

    TEST_ASSERT_EQUAL_INT(CAN_PORT_OK, can_service_send(&f, 100));
    TEST_ASSERT_EQUAL_INT(CAN_PORT_OK, can_service_send(&f, 100));
    can_service_on_rx(&f);
    can_service_get_stats(&s);

    TEST_ASSERT_EQUAL_UINT32(2, s.tx_ok);
    TEST_ASSERT_EQUAL_UINT32(0, s.tx_err);
    TEST_ASSERT_EQUAL_UINT32(1, s.rx);
}

static void test_stats_count_tx_errors(void)
{
    can_frame_t f = { .id = 0x10, .dlc = 0 };
    can_stats_t s;
    fake_can_reset(FAKE_CAN_TX_FAIL);

    TEST_ASSERT_EQUAL_INT(CAN_PORT_ERR, can_service_send(&f, 100));
    can_service_get_stats(&s);

    TEST_ASSERT_EQUAL_UINT32(0, s.tx_ok);
    TEST_ASSERT_EQUAL_UINT32(1, s.tx_err);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_make_test_frame_pattern);
    RUN_TEST(test_frames_equal_identical);
    RUN_TEST(test_frames_equal_detects_id_difference);
    RUN_TEST(test_frames_equal_detects_data_difference);
    RUN_TEST(test_frames_equal_ignores_bytes_beyond_dlc);

    RUN_TEST(test_selftest_passes_with_loopback);
    RUN_TEST(test_selftest_counts_timeouts_when_frames_dropped);
    RUN_TEST(test_selftest_counts_mismatches_when_corrupted);
    RUN_TEST(test_selftest_counts_tx_errors);
    RUN_TEST(test_selftest_drains_old_frames_first);
    RUN_TEST(test_selftest_rejects_invalid_frame_counts);

    RUN_TEST(test_parse_standard_id_without_data);
    RUN_TEST(test_parse_id_with_0x_prefix_and_data);
    RUN_TEST(test_parse_id_above_7ff_is_extended);
    RUN_TEST(test_parse_max_extended_id);
    RUN_TEST(test_parse_missing_id);
    RUN_TEST(test_parse_bad_ids);
    RUN_TEST(test_parse_bad_bytes);
    RUN_TEST(test_parse_eight_bytes_ok_nine_rejected);

    RUN_TEST(test_format_standard_frame);
    RUN_TEST(test_format_extended_frame_without_data);
    RUN_TEST(test_format_small_buffer_is_truncated_safely);

    RUN_TEST(test_stats_start_at_zero);
    RUN_TEST(test_stats_count_tx_ok_and_rx);
    RUN_TEST(test_stats_count_tx_errors);

    return UNITY_END();
}
