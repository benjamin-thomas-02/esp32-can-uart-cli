#include "unity.h"
#include "cli.h"

static cli_line_t s_line;

void setUp(void)
{
    cli_line_init(&s_line);
}

void tearDown(void)
{
}

/* Helper: feed a whole string, one character at a time. */
static void feed_string(const char *s)
{
    while (*s != '\0') {
        (void)cli_line_feed(&s_line, *s);
        s++;
    }
}

static void test_printable_chars_are_stored(void)
{
    feed_string("abc");

    TEST_ASSERT_EQUAL_UINT(3, s_line.len);
    TEST_ASSERT_EQUAL_STRING("abc", s_line.buf);
}

static void test_carriage_return_completes_line(void)
{
    feed_string("help");

    TEST_ASSERT_EQUAL_INT(CLI_LINE_READY, cli_line_feed(&s_line, '\r'));
    TEST_ASSERT_EQUAL_STRING("help", s_line.buf);
}

static void test_newline_completes_line(void)
{
    feed_string("help");

    TEST_ASSERT_EQUAL_INT(CLI_LINE_READY, cli_line_feed(&s_line, '\n'));
}

static void test_crlf_counts_as_one_enter(void)
{
    feed_string("ab");

    TEST_ASSERT_EQUAL_INT(CLI_LINE_READY, cli_line_feed(&s_line, '\r'));
    TEST_ASSERT_EQUAL_INT(CLI_LINE_NONE,  cli_line_feed(&s_line, '\n'));
}

static void test_backspace_removes_last_char(void)
{
    feed_string("abc");

    cli_line_feed(&s_line, '\b');

    TEST_ASSERT_EQUAL_UINT(2, s_line.len);
    TEST_ASSERT_EQUAL_STRING("ab", s_line.buf);
}

static void test_del_also_works_as_backspace(void)
{
    feed_string("abc");

    cli_line_feed(&s_line, 0x7F);

    TEST_ASSERT_EQUAL_STRING("ab", s_line.buf);
}

static void test_backspace_on_empty_line_does_nothing(void)
{
    TEST_ASSERT_EQUAL_INT(CLI_LINE_NONE, cli_line_feed(&s_line, '\b'));
    TEST_ASSERT_EQUAL_UINT(0, s_line.len);
}

static void test_non_printable_chars_are_ignored(void)
{
    cli_line_feed(&s_line, 0x1B);   /* ESC */
    cli_line_feed(&s_line, 0x01);   /* Ctrl+A */

    TEST_ASSERT_EQUAL_UINT(0, s_line.len);
}

static void test_overflow_drops_extra_chars(void)
{
    for (int i = 0; i < CLI_MAX_LINE - 1; i++) {          /* fill 127 chars */
        TEST_ASSERT_EQUAL_INT(CLI_LINE_NONE, cli_line_feed(&s_line, 'x'));
    }

    TEST_ASSERT_EQUAL_INT(CLI_LINE_OVERFLOW, cli_line_feed(&s_line, 'y'));
    TEST_ASSERT_EQUAL_UINT(CLI_MAX_LINE - 1, s_line.len);
    TEST_ASSERT_EQUAL_CHAR('\0', s_line.buf[CLI_MAX_LINE - 1]);   /* still terminated */
}

static void test_clear_keeps_crlf_state(void)
{
    feed_string("a");
    TEST_ASSERT_EQUAL_INT(CLI_LINE_READY, cli_line_feed(&s_line, '\r'));

    cli_line_clear(&s_line);                                      /* like uart_rx_task does */

    TEST_ASSERT_EQUAL_INT(CLI_LINE_NONE, cli_line_feed(&s_line, '\n'));
    TEST_ASSERT_EQUAL_UINT(0, s_line.len);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_printable_chars_are_stored);
    RUN_TEST(test_carriage_return_completes_line);
    RUN_TEST(test_newline_completes_line);
    RUN_TEST(test_crlf_counts_as_one_enter);
    RUN_TEST(test_backspace_removes_last_char);
    RUN_TEST(test_del_also_works_as_backspace);
    RUN_TEST(test_backspace_on_empty_line_does_nothing);
    RUN_TEST(test_non_printable_chars_are_ignored);
    RUN_TEST(test_overflow_drops_extra_chars);
    RUN_TEST(test_clear_keeps_crlf_state);

    return UNITY_END();
}
