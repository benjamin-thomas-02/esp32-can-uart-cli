#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stddef.h>

#define CLI_MAX_LINE      128   /* max characters in one command line (incl. '\0') */
#define CLI_MAX_ARGS      8     /* max words in one command line */
#define CLI_MAX_COMMANDS  16    /* max registered commands */

typedef enum {
    CLI_OK = 0,
    CLI_ERR_EMPTY,          /* line had no words */
    CLI_ERR_TOO_MANY_ARGS,  /* more than CLI_MAX_ARGS words */
    CLI_ERR_UNKNOWN_CMD,    /* first word is not a registered command */
    CLI_ERR_REGISTRY_FULL,  /* no space to register more commands */
    CLI_ERR_INVALID_ARG     /* bad argument (NULL pointer, wrong parameters) */
} cli_status_t;

/* A command handler gets the words of the line, like main(argc, argv). */
typedef cli_status_t (*cli_handler_t)(int argc, char *argv[]);

typedef struct {
    const char    *name;     /* word the user types, e.g. "version" */
    const char    *help;     /* one-line description for 'help' */
    cli_handler_t  handler;  /* function to run */
} cli_command_t;

/* Function the CLI uses to print text (injected by the app). */
typedef void (*cli_write_fn)(const char *text);

/* ---- command registry / execution (cli.c) ---- */
void         cli_init(cli_write_fn write_fn);
cli_status_t cli_register(const cli_command_t *cmd);
cli_status_t cli_execute(char *line);
int          cli_tokenize(char *line, char *argv[], int max_args);
void         cli_print(const char *text);
const char  *cli_status_str(cli_status_t status);

/* ---- line buffer (cli_line.c) ---- */
typedef struct {
    char   buf[CLI_MAX_LINE];
    size_t len;
    bool   last_was_cr;      /* to ignore the '\n' of a "\r\n" pair */
} cli_line_t;

typedef enum {
    CLI_LINE_NONE = 0,       /* character handled, line not finished */
    CLI_LINE_READY,          /* Enter pressed: buf holds a complete line */
    CLI_LINE_OVERFLOW        /* line full, character dropped */
} cli_line_result_t;

void              cli_line_init(cli_line_t *line);
void              cli_line_clear(cli_line_t *line);
cli_line_result_t cli_line_feed(cli_line_t *line, char c);

#endif /* CLI_H */
