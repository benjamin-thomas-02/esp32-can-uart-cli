#include "cli.h"

#include <stdio.h>
#include <string.h>

static const cli_command_t *s_commands[CLI_MAX_COMMANDS];
static size_t               s_command_count;
static cli_write_fn         s_write;

static cli_status_t cmd_help(int argc, char *argv[]);

static const cli_command_t s_help_cmd = {
    .name    = "help",
    .help    = "List all commands",
    .handler = cmd_help,
};

void cli_init(cli_write_fn write_fn)
{
    s_write         = write_fn;
    s_command_count = 0;
    (void)cli_register(&s_help_cmd);
}

void cli_print(const char *text)
{
    if (s_write != NULL && text != NULL) {
        s_write(text);
    }
}

cli_status_t cli_register(const cli_command_t *cmd)
{
    if (cmd == NULL || cmd->name == NULL || cmd->handler == NULL) {
        return CLI_ERR_INVALID_ARG;
    }
    if (s_command_count >= CLI_MAX_COMMANDS) {
        return CLI_ERR_REGISTRY_FULL;
    }
    s_commands[s_command_count++] = cmd;
    return CLI_OK;
}

/* Split a line into words, in place. Spaces/tabs are replaced by '\0'.
 * Returns number of words, or -1 if there are more than max_args. */
int cli_tokenize(char *line, char *argv[], int max_args)
{
    int   argc = 0;
    char *p    = line;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') {   /* skip separators */
            *p = '\0';
            p++;
        }
        if (*p == '\0') {
            break;                          /* end of line */
        }
        if (argc >= max_args) {
            return -1;                      /* too many words */
        }
        argv[argc++] = p;                   /* start of a word */
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;                            /* walk to end of word */
        }
    }
    return argc;
}

cli_status_t cli_execute(char *line)
{
    char *argv[CLI_MAX_ARGS];

    if (line == NULL) {
        return CLI_ERR_INVALID_ARG;
    }

    int argc = cli_tokenize(line, argv, CLI_MAX_ARGS);
    if (argc < 0) {
        return CLI_ERR_TOO_MANY_ARGS;
    }
    if (argc == 0) {
        return CLI_ERR_EMPTY;
    }

    for (size_t i = 0; i < s_command_count; i++) {
        if (strcmp(argv[0], s_commands[i]->name) == 0) {
            return s_commands[i]->handler(argc, argv);
        }
    }
    return CLI_ERR_UNKNOWN_CMD;
}

const char *cli_status_str(cli_status_t status)
{
    switch (status) {
    case CLI_OK:                return "ok";
    case CLI_ERR_EMPTY:         return "empty line";
    case CLI_ERR_TOO_MANY_ARGS: return "too many arguments";
    case CLI_ERR_UNKNOWN_CMD:   return "unknown command";
    case CLI_ERR_REGISTRY_FULL: return "command registry full";
    case CLI_ERR_INVALID_ARG:   return "invalid argument";
    default:                    return "unknown error";
    }
}

static cli_status_t cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    char out[96];

    for (size_t i = 0; i < s_command_count; i++) {
        snprintf(out, sizeof(out), "  %-10s %s\r\n",
                 s_commands[i]->name,
                 (s_commands[i]->help != NULL) ? s_commands[i]->help : "");
        cli_print(out);
    }
    return CLI_OK;
}
