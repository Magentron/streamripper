/* test_charset.c - Unit tests for lib/charset.c */
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "charset.h"

void setUp(void)
{
    /* No mocks needed - testing pure charset conversion functions */
}

void tearDown(void)
{
    /* Nothing to tear down */
}

/* =================================================================
 * Tests for charset_find()
 * ================================================================= */

static void test_charset_find_utf8(void)
{
    struct charset *cs = charset_find("UTF-8");
    TEST_ASSERT_NOT_NULL(cs);
}

static void test_charset_find_utf8_lowercase(void)
{
    struct charset *cs = charset_find("utf-8");
    TEST_ASSERT_NOT_NULL(cs);
}

static void test_charset_find_us_ascii(void)
{
    struct charset *cs = charset_find("US-ASCII");
    TEST_ASSERT_NOT_NULL(cs);
}

static void test_charset_find_iso_8859_1(void)
{
    struct charset *cs = charset_find("ISO-8859-1");
    TEST_ASSERT_NOT_NULL(cs);
}

static void test_charset_find_latin1_alias(void)
{
    /* latin1 alias may not be supported - this tests the actual behavior */
    /* The implementation only recognizes specific encoding names */
    struct charset *cs = charset_find("latin1");
    /* Note: This implementation does NOT support the "latin1" alias,
       it requires the full name "ISO-8859-1" */
    TEST_ASSERT_NULL(cs);
}

static void test_charset_find_unknown_returns_null(void)
{
    struct charset *cs = charset_find("UNKNOWN-CHARSET-XYZ");
    TEST_ASSERT_NULL(cs);
}

/* =================================================================
 * Tests for charset_max()
 * ================================================================= */

static void test_charset_max_utf8(void)
{
    struct charset *cs = charset_find("UTF-8");
    TEST_ASSERT_NOT_NULL(cs);
    /* UTF-8 can be up to 6 bytes per character */
    TEST_ASSERT_EQUAL(6, charset_max(cs));
}

static void test_charset_max_ascii(void)
{
    struct charset *cs = charset_find("US-ASCII");
    TEST_ASSERT_NOT_NULL(cs);
    /* ASCII is always 1 byte */
    TEST_ASSERT_EQUAL(1, charset_max(cs));
}

static void test_charset_max_iso_8859_1(void)
{
    struct charset *cs = charset_find("ISO-8859-1");
    TEST_ASSERT_NOT_NULL(cs);
    /* ISO-8859-1 is always 1 byte */
    TEST_ASSERT_EQUAL(1, charset_max(cs));
}

/* =================================================================
 * Tests for utf8_mbtowc() - multibyte to wide char
 * ================================================================= */

static void test_utf8_mbtowc_ascii_char(void)
{
    int wc;
    int result = utf8_mbtowc(&wc, "A", 1);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL('A', wc);
}

static void test_utf8_mbtowc_null_byte(void)
{
    int wc;
    int result = utf8_mbtowc(&wc, "\0", 1);
    TEST_ASSERT_EQUAL(0, result);  /* Returns 0 for null byte */
}

static void test_utf8_mbtowc_two_byte_sequence(void)
{
    /* UTF-8 for U+00E9 (e with acute accent): 0xC3 0xA9 */
    int wc;
    const char utf8[] = {0xC3, 0xA9, 0};
    int result = utf8_mbtowc(&wc, utf8, 2);
    TEST_ASSERT_EQUAL(2, result);
    TEST_ASSERT_EQUAL(0xE9, wc);  /* e with acute */
}

static void test_utf8_mbtowc_three_byte_sequence(void)
{
    /* UTF-8 for Euro sign U+20AC: 0xE2 0x82 0xAC */
    int wc;
    const char utf8[] = {0xE2, 0x82, 0xAC, 0};
    int result = utf8_mbtowc(&wc, utf8, 3);
    TEST_ASSERT_EQUAL(3, result);
    TEST_ASSERT_EQUAL(0x20AC, wc);  /* Euro sign */
}

static void test_utf8_mbtowc_four_byte_sequence(void)
{
    /* UTF-8 for U+1F600 (grinning face): 0xF0 0x9F 0x98 0x80 */
    int wc;
    const char utf8[] = {0xF0, 0x9F, 0x98, 0x80, 0};
    int result = utf8_mbtowc(&wc, utf8, 4);
    TEST_ASSERT_EQUAL(4, result);
    TEST_ASSERT_EQUAL(0x1F600, wc);
}

static void test_utf8_mbtowc_invalid_continuation(void)
{
    int wc;
    /* Invalid: starts with continuation byte */
    const char utf8[] = {0x80, 0};
    int result = utf8_mbtowc(&wc, utf8, 1);
    TEST_ASSERT_EQUAL(-1, result);  /* Invalid sequence */
}

static void test_utf8_mbtowc_truncated_sequence(void)
{
    int wc;
    /* Two-byte sequence but only 1 byte available */
    const char utf8[] = {0xC3, 0};
    int result = utf8_mbtowc(&wc, utf8, 1);
    TEST_ASSERT_EQUAL(-1, result);  /* Incomplete sequence */
}

static void test_utf8_mbtowc_null_string(void)
{
    int wc;
    int result = utf8_mbtowc(&wc, NULL, 0);
    TEST_ASSERT_EQUAL(0, result);
}

static void test_utf8_mbtowc_zero_length(void)
{
    int wc;
    int result = utf8_mbtowc(&wc, "A", 0);
    TEST_ASSERT_EQUAL(0, result);
}

/* =================================================================
 * Tests for utf8_wctomb() - wide char to multibyte
 * ================================================================= */

static void test_utf8_wctomb_ascii_char(void)
{
    char buf[7] = {0};
    int result = utf8_wctomb(buf, 'A');
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL('A', buf[0]);
}

static void test_utf8_wctomb_two_byte_char(void)
{
    char buf[7] = {0};
    int result = utf8_wctomb(buf, 0xE9);  /* e with acute */
    TEST_ASSERT_EQUAL(2, result);
    TEST_ASSERT_EQUAL((char)0xC3, buf[0]);
    TEST_ASSERT_EQUAL((char)0xA9, buf[1]);
}

static void test_utf8_wctomb_three_byte_char(void)
{
    char buf[7] = {0};
    int result = utf8_wctomb(buf, 0x20AC);  /* Euro sign */
    TEST_ASSERT_EQUAL(3, result);
    TEST_ASSERT_EQUAL((char)0xE2, buf[0]);
    TEST_ASSERT_EQUAL((char)0x82, buf[1]);
    TEST_ASSERT_EQUAL((char)0xAC, buf[2]);
}

static void test_utf8_wctomb_four_byte_char(void)
{
    char buf[7] = {0};
    int result = utf8_wctomb(buf, 0x1F600);  /* Grinning face emoji */
    TEST_ASSERT_EQUAL(4, result);
    TEST_ASSERT_EQUAL((char)0xF0, buf[0]);
    TEST_ASSERT_EQUAL((char)0x9F, buf[1]);
    TEST_ASSERT_EQUAL((char)0x98, buf[2]);
    TEST_ASSERT_EQUAL((char)0x80, buf[3]);
}

static void test_utf8_wctomb_null_buffer(void)
{
    int result = utf8_wctomb(NULL, 'A');
    TEST_ASSERT_EQUAL(0, result);
}

/* =================================================================
 * Tests for charset_mbtowc() / charset_wctomb() via charset object
 * ================================================================= */

static void test_charset_mbtowc_utf8(void)
{
    struct charset *cs = charset_find("UTF-8");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    const char utf8[] = {0xC3, 0xA9, 0};  /* e with acute */
    int result = charset_mbtowc(cs, &wc, utf8, 2);
    TEST_ASSERT_EQUAL(2, result);
    TEST_ASSERT_EQUAL(0xE9, wc);
}

static void test_charset_wctomb_utf8(void)
{
    struct charset *cs = charset_find("UTF-8");
    TEST_ASSERT_NOT_NULL(cs);

    char buf[7] = {0};
    int result = charset_wctomb(cs, buf, 0xE9);  /* e with acute */
    TEST_ASSERT_EQUAL(2, result);
    TEST_ASSERT_EQUAL((char)0xC3, buf[0]);
    TEST_ASSERT_EQUAL((char)0xA9, buf[1]);
}

static void test_charset_mbtowc_ascii(void)
{
    struct charset *cs = charset_find("US-ASCII");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    int result = charset_mbtowc(cs, &wc, "X", 1);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL('X', wc);
}

static void test_charset_mbtowc_ascii_high_bit_invalid(void)
{
    struct charset *cs = charset_find("US-ASCII");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    const char high_bit[] = {0x80, 0};
    int result = charset_mbtowc(cs, &wc, high_bit, 1);
    TEST_ASSERT_EQUAL(-1, result);  /* Invalid for ASCII */
}

static void test_charset_mbtowc_iso_8859_1(void)
{
    struct charset *cs = charset_find("ISO-8859-1");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    const char latin1[] = {0xE9, 0};  /* e with acute in ISO-8859-1 */
    int result = charset_mbtowc(cs, &wc, latin1, 1);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL(0xE9, wc);
}

/* =================================================================
 * Tests for charset_convert()
 * ================================================================= */

static void test_charset_convert_utf8_to_ascii_simple(void)
{
    char *output = NULL;
    size_t outlen = 0;

    int result = charset_convert("UTF-8", "US-ASCII", "Hello", 5, &output, &outlen);

    TEST_ASSERT_EQUAL(0, result);  /* Exact conversion */
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("Hello", output);
    TEST_ASSERT_EQUAL(5, outlen);

    free(output);
}

static void test_charset_convert_utf8_to_ascii_with_non_ascii(void)
{
    char *output = NULL;
    size_t outlen = 0;

    /* "cafe" with accented e (UTF-8: caf\xC3\xA9) */
    const char utf8_input[] = {'c', 'a', 'f', 0xC3, 0xA9, 0};

    int result = charset_convert("UTF-8", "US-ASCII", utf8_input, 5, &output, &outlen);

    /* Result 1 means approximate conversion (using '?') */
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_NOT_NULL(output);
    /* The accented e should be replaced with '?' */
    TEST_ASSERT_EQUAL_STRING("caf?", output);

    free(output);
}

static void test_charset_convert_ascii_to_utf8(void)
{
    char *output = NULL;
    size_t outlen = 0;

    int result = charset_convert("US-ASCII", "UTF-8", "test", 4, &output, &outlen);

    TEST_ASSERT_EQUAL(0, result);  /* Exact conversion */
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("test", output);
    TEST_ASSERT_EQUAL(4, outlen);

    free(output);
}

static void test_charset_convert_iso_8859_1_to_utf8(void)
{
    char *output = NULL;
    size_t outlen = 0;

    /* e with acute in ISO-8859-1 (0xE9) */
    const char latin1[] = {0xE9, 0};

    int result = charset_convert("ISO-8859-1", "UTF-8", latin1, 1, &output, &outlen);

    TEST_ASSERT_EQUAL(0, result);  /* Exact conversion */
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL(2, outlen);  /* UTF-8 uses 2 bytes for this char */
    TEST_ASSERT_EQUAL((char)0xC3, output[0]);
    TEST_ASSERT_EQUAL((char)0xA9, output[1]);

    free(output);
}

static void test_charset_convert_utf8_to_iso_8859_1(void)
{
    char *output = NULL;
    size_t outlen = 0;

    /* e with acute in UTF-8 */
    const char utf8[] = {0xC3, 0xA9, 0};

    int result = charset_convert("UTF-8", "ISO-8859-1", utf8, 2, &output, &outlen);

    TEST_ASSERT_EQUAL(0, result);  /* Exact conversion */
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL(1, outlen);  /* ISO-8859-1 uses 1 byte */
    TEST_ASSERT_EQUAL((char)0xE9, output[0]);

    free(output);
}

static void test_charset_convert_invalid_input_bytes(void)
{
    char *output = NULL;
    size_t outlen = 0;

    /* Invalid UTF-8 sequence (continuation byte without lead byte) */
    const char invalid_utf8[] = {0x80, 0x80, 0};

    int result = charset_convert("UTF-8", "US-ASCII", invalid_utf8, 2, &output, &outlen);

    /* Result 2 means invalid input (converted using '#') */
    TEST_ASSERT_EQUAL(2, result);
    TEST_ASSERT_NOT_NULL(output);
    /* Invalid bytes should be replaced with '#' */
    TEST_ASSERT_EQUAL_STRING("##", output);

    free(output);
}

static void test_charset_convert_unknown_encoding(void)
{
    char *output = NULL;
    size_t outlen = 0;

    int result = charset_convert("UNKNOWN-CHARSET", "UTF-8", "test", 4, &output, &outlen);

    TEST_ASSERT_EQUAL(-1, result);  /* Unknown encoding */
}

static void test_charset_convert_empty_string(void)
{
    char *output = NULL;
    size_t outlen = 0;

    int result = charset_convert("UTF-8", "US-ASCII", "", 0, &output, &outlen);

    TEST_ASSERT_EQUAL(0, result);  /* Exact conversion */
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("", output);
    TEST_ASSERT_EQUAL(0, outlen);

    free(output);
}

static void test_charset_convert_null_output_allowed(void)
{
    /* When to is NULL, the function should still work (compute length) */
    size_t outlen = 0;

    int result = charset_convert("UTF-8", "US-ASCII", "test", 4, NULL, &outlen);

    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(4, outlen);
}

/* =================================================================
 * Main entry point
 * ================================================================= */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* charset_find tests */
    RUN_TEST(test_charset_find_utf8);
    RUN_TEST(test_charset_find_utf8_lowercase);
    RUN_TEST(test_charset_find_us_ascii);
    RUN_TEST(test_charset_find_iso_8859_1);
    RUN_TEST(test_charset_find_latin1_alias);
    RUN_TEST(test_charset_find_unknown_returns_null);

    /* charset_max tests */
    RUN_TEST(test_charset_max_utf8);
    RUN_TEST(test_charset_max_ascii);
    RUN_TEST(test_charset_max_iso_8859_1);

    /* utf8_mbtowc tests */
    RUN_TEST(test_utf8_mbtowc_ascii_char);
    RUN_TEST(test_utf8_mbtowc_null_byte);
    RUN_TEST(test_utf8_mbtowc_two_byte_sequence);
    RUN_TEST(test_utf8_mbtowc_three_byte_sequence);
    RUN_TEST(test_utf8_mbtowc_four_byte_sequence);
    RUN_TEST(test_utf8_mbtowc_invalid_continuation);
    RUN_TEST(test_utf8_mbtowc_truncated_sequence);
    RUN_TEST(test_utf8_mbtowc_null_string);
    RUN_TEST(test_utf8_mbtowc_zero_length);

    /* utf8_wctomb tests */
    RUN_TEST(test_utf8_wctomb_ascii_char);
    RUN_TEST(test_utf8_wctomb_two_byte_char);
    RUN_TEST(test_utf8_wctomb_three_byte_char);
    RUN_TEST(test_utf8_wctomb_four_byte_char);
    RUN_TEST(test_utf8_wctomb_null_buffer);

    /* charset_mbtowc / charset_wctomb tests */
    RUN_TEST(test_charset_mbtowc_utf8);
    RUN_TEST(test_charset_wctomb_utf8);
    RUN_TEST(test_charset_mbtowc_ascii);
    RUN_TEST(test_charset_mbtowc_ascii_high_bit_invalid);
    RUN_TEST(test_charset_mbtowc_iso_8859_1);

    /* charset_convert tests */
    RUN_TEST(test_charset_convert_utf8_to_ascii_simple);
    RUN_TEST(test_charset_convert_utf8_to_ascii_with_non_ascii);
    RUN_TEST(test_charset_convert_ascii_to_utf8);
    RUN_TEST(test_charset_convert_iso_8859_1_to_utf8);
    RUN_TEST(test_charset_convert_utf8_to_iso_8859_1);
    RUN_TEST(test_charset_convert_invalid_input_bytes);
    RUN_TEST(test_charset_convert_unknown_encoding);
    RUN_TEST(test_charset_convert_empty_string);
    RUN_TEST(test_charset_convert_null_output_allowed);

    return UNITY_END();
}
