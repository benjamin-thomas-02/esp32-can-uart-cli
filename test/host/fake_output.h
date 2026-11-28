#ifndef FAKE_OUTPUT_H
#define FAKE_OUTPUT_H

/* Fake replacement for console_write(): stores text instead of sending to UART. */
void        fake_output_reset(void);
void        fake_output_write(const char *text);
const char *fake_output_get(void);

#endif /* FAKE_OUTPUT_H */
