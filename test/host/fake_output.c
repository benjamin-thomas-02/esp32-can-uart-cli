#include "fake_output.h"

#include <stddef.h>
#include <string.h>

#define FAKE_OUTPUT_SIZE 2048

static char   s_buf[FAKE_OUTPUT_SIZE];
static size_t s_len;

void fake_output_reset(void)
{
    s_len    = 0;
    s_buf[0] = '\0';
}

void fake_output_write(const char *text)
{
    size_t n = strlen(text);

    if (s_len + n >= FAKE_OUTPUT_SIZE) {
        n = FAKE_OUTPUT_SIZE - 1 - s_len;   /* truncate, never overflow */
    }
    memcpy(&s_buf[s_len], text, n);
    s_len += n;
    s_buf[s_len] = '\0';
}

const char *fake_output_get(void)
{
    return s_buf;
}
