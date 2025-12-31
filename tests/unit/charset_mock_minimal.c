/* Minimal mock for charset.c - only mocks charset_convert for error testing */
#include <stdlib.h>
#include <string.h>
#include "charset.h"

/* Mock control variables */
static int mock_charset_convert_return_value = 0;
static int mock_charset_convert_enabled = 0;

/* Real charset.c functions that we need (forward declarations) */
extern int charset_convert_real(
    const char *fromcode,
    const char *tocode,
    const char *from,
    size_t fromlen,
    char **to,
    size_t *tolen);

/* Mock control functions */
void charset_convert_mock_enable(int return_value)
{
    mock_charset_convert_enabled = 1;
    mock_charset_convert_return_value = return_value;
}

void charset_convert_mock_disable(void)
{
    mock_charset_convert_enabled = 0;
}

/* Override charset_convert - calls real or returns mock value */
int charset_convert(
    const char *fromcode,
    const char *tocode,
    const char *from,
    size_t fromlen,
    char **to,
    size_t *tolen)
{
    if (mock_charset_convert_enabled) {
        return mock_charset_convert_return_value;
    }

    /* Call the real implementation */
    return charset_convert_real(fromcode, tocode, from, fromlen, to, tolen);
}
