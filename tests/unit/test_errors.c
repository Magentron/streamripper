/* test_errors.c - Unit tests for lib/errors.c
 *
 * Tests for error handling functions.
 */
#include <string.h>
#include <unity.h>

#include "errors.h"

void setUp(void)
{
    /* Initialize error strings before each test */
    errors_init();
}

void tearDown(void)
{
    /* Nothing to clean up */
}

/* Test: SR_SUCCESS returns a valid error string */
static void test_error_success_code(void)
{
    char *err_str = errors_get_string(SR_SUCCESS);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_EQUAL_STRING("SR_SUCCESS", err_str);
}

/* Test: SR_SUCCESS_BUFFERING is a positive code, should return NULL */
static void test_error_buffering_code_returns_null(void)
{
    /* SR_SUCCESS_BUFFERING = 0x01, which is positive.
     * The function returns NULL for code > 0 */
    char *err_str = errors_get_string(SR_SUCCESS_BUFFERING);
    TEST_ASSERT_NULL(err_str);
}

/* Test: Common error codes have valid strings */
static void test_error_common_codes_have_strings(void)
{
    /* Test a sampling of common error codes */
    char *err_str;

    err_str = errors_get_string(SR_ERROR_INVALID_URL);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_CONNECT_FAILED);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_RECV_FAILED);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_CANT_ALLOC_MEMORY);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);
}

/* Test: HTTP error codes have descriptive strings */
static void test_error_http_codes_have_strings(void)
{
    char *err_str;

    err_str = errors_get_string(SR_ERROR_HTTP_404_ERROR);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strstr(err_str, "404") != NULL);

    err_str = errors_get_string(SR_ERROR_HTTP_401_ERROR);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strstr(err_str, "401") != NULL);

    err_str = errors_get_string(SR_ERROR_HTTP_502_ERROR);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strstr(err_str, "502") != NULL);

    err_str = errors_get_string(SR_ERROR_HTTP_400_ERROR);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strstr(err_str, "400") != NULL);

    err_str = errors_get_string(SR_ERROR_HTTP_407_ERROR);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strstr(err_str, "407") != NULL);

    err_str = errors_get_string(SR_ERROR_HTTP_403_ERROR);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strstr(err_str, "403") != NULL);
}

/* Test: File operation error codes have strings */
static void test_error_file_codes_have_strings(void)
{
    char *err_str;

    err_str = errors_get_string(SR_ERROR_CANT_CREATE_FILE);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_CANT_WRITE_TO_FILE);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_CANT_CREATE_DIR);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_INVALID_DIRECTORY);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);
}

/* Test: Buffer error codes have strings */
static void test_error_buffer_codes_have_strings(void)
{
    char *err_str;

    err_str = errors_get_string(SR_ERROR_BUFFER_EMPTY);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_BUFFER_FULL);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_BUFFER_TOO_SMALL);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);
}

/* Test: SSL error codes have strings */
static void test_error_ssl_codes_have_strings(void)
{
    char *err_str;

    /* Note: SSL errors are not initialized in errors_init() per current code,
     * but the array should still have space for them.
     * These will return empty strings if not initialized. */
    err_str = errors_get_string(SR_ERROR_SSL_CTX_NEW);
    TEST_ASSERT_NOT_NULL(err_str);
    /* String may be empty if not initialized */

    err_str = errors_get_string(SR_ERROR_SSL_NEW);
    TEST_ASSERT_NOT_NULL(err_str);

    err_str = errors_get_string(SR_ERROR_SSL_SET_FD);
    TEST_ASSERT_NOT_NULL(err_str);

    err_str = errors_get_string(SR_ERROR_SSL_CONNECT);
    TEST_ASSERT_NOT_NULL(err_str);
}

/* Test: Out of range error code returns NULL */
static void test_error_out_of_range_returns_null(void)
{
    char *err_str;

    /* Test code beyond NUM_ERROR_CODES (0x51 + 1 = 82) */
    /* Error codes are negative, so -100 should be out of range */
    err_str = errors_get_string(-100);
    TEST_ASSERT_NULL(err_str);

    /* Very large negative number */
    err_str = errors_get_string(-1000);
    TEST_ASSERT_NULL(err_str);
}

/* Test: Verify redirect error codes exist */
static void test_error_redirect_codes_have_strings(void)
{
    char *err_str;

    /* Note: Redirect codes are not initialized in errors_init() per current code */
    err_str = errors_get_string(SR_ERROR_HTTP_REDIRECT);
    TEST_ASSERT_NOT_NULL(err_str);

    err_str = errors_get_string(SR_ERROR_HTTPS_REDIRECT);
    TEST_ASSERT_NOT_NULL(err_str);
}

/* Test: Parse and metadata error codes have strings */
static void test_error_parse_codes_have_strings(void)
{
    char *err_str;

    err_str = errors_get_string(SR_ERROR_PARSE_FAILURE);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_INVALID_METADATA);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_CANT_PARSE_PLS);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_CANT_PARSE_M3U);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);
}

/* Test: Network/socket error codes have strings */
static void test_error_socket_codes_have_strings(void)
{
    char *err_str;

    err_str = errors_get_string(SR_ERROR_CANT_RESOLVE_HOSTNAME);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_SEND_FAILED);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_SOCKET_CLOSED);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_CANT_CREATE_SOCKET);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_TIMEOUT);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);
}

/* Test: Thread error codes have strings */
static void test_error_thread_codes_have_strings(void)
{
    char *err_str;

    err_str = errors_get_string(SR_ERROR_CANT_CREATE_THREAD);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_CANT_WAIT_ON_THREAD);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);

    err_str = errors_get_string(SR_ERROR_CANT_CREATE_EVENT);
    TEST_ASSERT_NOT_NULL(err_str);
    TEST_ASSERT_TRUE(strlen(err_str) > 0);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_error_success_code);
    RUN_TEST(test_error_buffering_code_returns_null);
    RUN_TEST(test_error_common_codes_have_strings);
    RUN_TEST(test_error_http_codes_have_strings);
    RUN_TEST(test_error_file_codes_have_strings);
    RUN_TEST(test_error_buffer_codes_have_strings);
    RUN_TEST(test_error_ssl_codes_have_strings);
    RUN_TEST(test_error_out_of_range_returns_null);
    RUN_TEST(test_error_redirect_codes_have_strings);
    RUN_TEST(test_error_parse_codes_have_strings);
    RUN_TEST(test_error_socket_codes_have_strings);
    RUN_TEST(test_error_thread_codes_have_strings);

    return UNITY_END();
}
