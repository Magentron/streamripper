/* Testable version of charset.c with error injection support */

/* This file includes the real charset.c and adds error injection capabilities */
#include "charset_test_helper.h"

/* Error injection state */
static int inject_error = 0;
static int injected_error_code = 0;

void charset_convert_set_error(int error_code)
{
    inject_error = 1;
    injected_error_code = error_code;
}

void charset_convert_clear_error(void)
{
    inject_error = 0;
    injected_error_code = 0;
}

/* Wrapper for charset_convert that checks for error injection */
#define charset_convert charset_convert_original
#include "../../lib/charset.c"
#undef charset_convert

int charset_convert(
    const char *fromcode,
    const char *tocode,
    const char *from,
    size_t fromlen,
    char **to,
    size_t *tolen)
{
    /* If error injection is enabled, return the error code */
    if (inject_error) {
        return injected_error_code;
    }

    /* Otherwise call the original implementation */
    return charset_convert_original(fromcode, tocode, from, fromlen, to, tolen);
}
