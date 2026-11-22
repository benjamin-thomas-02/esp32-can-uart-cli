#include "cli.h"

#include <string.h>

void cli_line_init(cli_line_t *line)
{
    memset(line, 0, sizeof(*line));
}

void cli_line_clear(cli_line_t *line)
{
    line->len    = 0;
    line->buf[0] = '\0';
    /* last_was_cr is kept on purpose: the '\n' of "\r\n" may still arrive */
}

cli_line_result_t cli_line_feed(cli_line_t *line, char c)
{
    unsigned char uc = (unsigned char)c;

    /* "\r\n": we already finished the line on '\r', so skip this '\n' */
    if (uc == '\n' && line->last_was_cr) {
        line->last_was_cr = false;
        return CLI_LINE_NONE;
    }
    line->last_was_cr = (uc == '\r');

    if (uc == '\r' || uc == '\n') {              /* Enter */
        line->buf[line->len] = '\0';
        return CLI_LINE_READY;
    }

    if (uc == '\b' || uc == 0x7F) {             /* Backspace or DEL */
        if (line->len > 0) {
            line->len--;
            line->buf[line->len] = '\0';
        }
        return CLI_LINE_NONE;
    }

    if (uc < 0x20 || uc > 0x7E) {               /* ignore non-printable */
        return CLI_LINE_NONE;
    }

    if (line->len >= CLI_MAX_LINE - 1) {        /* keep 1 byte for '\0' */
        return CLI_LINE_OVERFLOW;
    }

    line->buf[line->len++] = (char)uc;
    line->buf[line->len]   = '\0';
    return CLI_LINE_NONE;
}
