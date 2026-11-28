#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "cli.h"
#include "fake_output.h"

/* ================= spy handler: records how it was called ================= */
static int  s_spy_calls;
static int  s_spy_argc;
static char s_spy_arg1[32];

static cli_status_t spy_handler(int argc, char *argv[])
{
    s_spy_calls++;
    s_spy_argc = argc;
    if (argc > 1) {
        snprintf(s_spy_arg1, sizeof(s_spy_arg1), "%s", argv[1]);
    }
    return CLI_OK;
}

static const cli_command_t s_spy_cmd = {
    .name    = "test",
    .help    = "Test command",
    .handler = spy_handler,
};

/* ================= runs before / after EVERY test ================= */
void setUp(void)
{
    fake_output_reset();
    s_spy_calls   = 0;
    s_spy_argc    = 0;
    s_spy_arg1[0] = '\0';
    cli_init(fake_output_write);        /* fresh registry with only 'help' */
}

void tearDown(void)
{
}

/* ================= tokenizer ================= */
static void test_tokenize_single_word(void)
{
    char  line[] = "version";
    char *argv[CLI_MAX_ARGS];

    int argc = cli_tokenize(line, argv, CLI_MAX_ARGS);

    TEST_ASSERT_EQUAL_INT(1, argc);
    TEST_ASSERT_EQUAL_STRING("version", argv[0]);
}

static void test_tokenize_multiple_spaces(void)
{
    char  line[] = "echo   hello    world";
    char *argv[CLI_MAX_ARGS];

    int argc = cli_tokenize(line, argv, CLI_MAX_ARGS);

    TEST_ASSERT_EQUAL_INT(3, argc);
    TEST_ASSERT_EQUAL_STRING("echo",  argv[0]);
    TEST_ASSERT_EQUAL_STRING("hello", argv[1]);
    TEST_ASSERT_EQUAL_STRING("world", argv[2]);
}

static void test_tokenize_leading_trailing_spaces_and_tabs(void)
{
    char  line[] = "  \techo\thi   ";
    char *argv[CLI_MAX_ARGS];

    int argc = cli_tokenize(line, argv, CLI_MAX_ARGS);

    TEST_ASSERT_EQUAL_INT(2, argc);
    TEST_ASSERT_EQUAL_STRING("echo", argv[0]);
    TEST_ASSERT_EQUAL_STRING("hi",   argv[1]);
}

static void test_tokenize_empty_and_blank_lines(void)
{
    char  empty[] = "";
    char  blank[] = "    ";
    char *argv[CLI_MAX_ARGS];

    TEST_ASSERT_EQUAL_INT(0, cli_tokenize(empty, argv, CLI_MAX_ARGS));
    TEST_ASSERT_EQUAL_INT(0, cli_tokenize(blank, argv, CLI_MAX_ARGS));
}

static void test_tokenize_max_args_is_allowed(void)
{
    char  line[] = "a b c d e f g h i j k l";            /* 12 words */
    char *argv[CLI_MAX_ARGS];

    TEST_ASSERT_EQUAL_INT(12, cli_tokenize(line, argv, CLI_MAX_ARGS));
}

static void test_tokenize_too_many_args(void)
{
    char  line[] = "a b c d e f g h i j k l m";          /* 13 words */
    char *argv[CLI_MAX_ARGS];

    TEST_ASSERT_EQUAL_INT(-1, cli_tokenize(line, argv, CLI_MAX_ARGS));
}

/* ================= execute ================= */
static void test_execute_calls_registered_handler(void)
{
    char line[] = "test hello";
    TEST_ASSERT_EQUAL_INT(CLI_OK, cli_register(&s_spy_cmd));

    cli_status_t st = cli_execute(line);

    TEST_ASSERT_EQUAL_INT(CLI_OK, st);
    TEST_ASSERT_EQUAL_INT(1, s_spy_calls);
    TEST_ASSERT_EQUAL_INT(2, s_spy_argc);
    TEST_ASSERT_EQUAL_STRING("hello", s_spy_arg1);
}

static void test_execute_unknown_command(void)
{
    char line[] = "foo";

    TEST_ASSERT_EQUAL_INT(CLI_ERR_UNKNOWN_CMD, cli_execute(line));
    TEST_ASSERT_EQUAL_INT(0, s_spy_calls);
}

static void test_execute_is_case_sensitive(void)
{
    char line[] = "TEST";
    cli_register(&s_spy_cmd);

    TEST_ASSERT_EQUAL_INT(CLI_ERR_UNKNOWN_CMD, cli_execute(line));
    TEST_ASSERT_EQUAL_INT(0, s_spy_calls);
}

static void test_execute_empty_line(void)
{
    char line[] = "   ";

    TEST_ASSERT_EQUAL_INT(CLI_ERR_EMPTY, cli_execute(line));
}

static void test_execute_too_many_args_does_not_call_handler(void)
{
    char line[] = "test 1 2 3 4 5 6 7 8 9 10 11 12";        /* 13 words */
    cli_register(&s_spy_cmd);

    TEST_ASSERT_EQUAL_INT(CLI_ERR_TOO_MANY_ARGS, cli_execute(line));
    TEST_ASSERT_EQUAL_INT(0, s_spy_calls);
}

static void test_execute_null_line(void)
{
    TEST_ASSERT_EQUAL_INT(CLI_ERR_INVALID_ARG, cli_execute(NULL));
}

/* ================= registry ================= */
static void test_register_null_is_rejected(void)
{
    TEST_ASSERT_EQUAL_INT(CLI_ERR_INVALID_ARG, cli_register(NULL));
}

static void test_register_without_handler_is_rejected(void)
{
    const cli_command_t bad = { .name = "bad", .help = "", .handler = NULL };

    TEST_ASSERT_EQUAL_INT(CLI_ERR_INVALID_ARG, cli_register(&bad));
}

static void test_registry_full(void)
{
    /* 'help' already uses 1 slot after cli_init() */
    for (int i = 0; i < CLI_MAX_COMMANDS - 1; i++) {
        TEST_ASSERT_EQUAL_INT(CLI_OK, cli_register(&s_spy_cmd));
    }
    TEST_ASSERT_EQUAL_INT(CLI_ERR_REGISTRY_FULL, cli_register(&s_spy_cmd));
}

/* ================= help / output ================= */
static void test_help_lists_registered_commands(void)
{
    char line[] = "help";
    cli_register(&s_spy_cmd);

    TEST_ASSERT_EQUAL_INT(CLI_OK, cli_execute(line));

    const char *out = fake_output_get();
    TEST_ASSERT_NOT_NULL(strstr(out, "help"));
    TEST_ASSERT_NOT_NULL(strstr(out, "test"));
    TEST_ASSERT_NOT_NULL(strstr(out, "Test command"));
}

static void test_no_output_function_does_not_crash(void)
{
    char line[] = "help";
    cli_init(NULL);

    TEST_ASSERT_EQUAL_INT(CLI_OK, cli_execute(line));
}

static void test_status_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("ok",              cli_status_str(CLI_OK));
    TEST_ASSERT_EQUAL_STRING("unknown command", cli_status_str(CLI_ERR_UNKNOWN_CMD));
}

/* ================= test runner ================= */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_tokenize_single_word);
    RUN_TEST(test_tokenize_multiple_spaces);
    RUN_TEST(test_tokenize_leading_trailing_spaces_and_tabs);
    RUN_TEST(test_tokenize_empty_and_blank_lines);
    RUN_TEST(test_tokenize_max_args_is_allowed);
    RUN_TEST(test_tokenize_too_many_args);

    RUN_TEST(test_execute_calls_registered_handler);
    RUN_TEST(test_execute_unknown_command);
    RUN_TEST(test_execute_is_case_sensitive);
    RUN_TEST(test_execute_empty_line);
    RUN_TEST(test_execute_too_many_args_does_not_call_handler);
    RUN_TEST(test_execute_null_line);

    RUN_TEST(test_register_null_is_rejected);
    RUN_TEST(test_register_without_handler_is_rejected);
    RUN_TEST(test_registry_full);

    RUN_TEST(test_help_lists_registered_commands);
    RUN_TEST(test_no_output_function_does_not_crash);
    RUN_TEST(test_status_strings);

    return UNITY_END();
}
