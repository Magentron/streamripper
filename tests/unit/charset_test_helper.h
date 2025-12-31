/* Test helper for charset.c - allows injecting errors into charset_convert */
#ifndef CHARSET_TEST_HELPER_H
#define CHARSET_TEST_HELPER_H

#include <stdlib.h>

/* Enable error injection for charset_convert */
void charset_convert_set_error(int error_code);

/* Disable error injection - return to normal behavior */
void charset_convert_clear_error(void);

#endif /* CHARSET_TEST_HELPER_H */
