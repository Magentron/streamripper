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

static void test_utf8_wctomb_five_byte_char(void)
{
    char buf[7] = {0};
    /* Code point requiring 5 bytes: U+200000 to U+3FFFFFF (2^21 to 2^26-1) */
    int result = utf8_wctomb(buf, 0x200000);  /* First 5-byte code point */
    TEST_ASSERT_EQUAL(5, result);
    TEST_ASSERT_EQUAL((char)0xF8, buf[0]);
}

static void test_utf8_wctomb_six_byte_char(void)
{
    char buf[7] = {0};
    /* Code point requiring 6 bytes: U+4000000 to U+7FFFFFFF (2^26 to 2^31-1) */
    int result = utf8_wctomb(buf, 0x4000000);  /* First 6-byte code point */
    TEST_ASSERT_EQUAL(6, result);
    TEST_ASSERT_EQUAL((char)0xFC, buf[0]);
}

static void test_utf8_wctomb_invalid_too_large(void)
{
    char buf[7] = {0};
    /* Code point too large for UTF-8 (>= 2^31) - uses unsigned int */
    /* We need to cast carefully since 0x80000000 is 2^31 */
    int result = utf8_wctomb(buf, (int)0x80000000);
    TEST_ASSERT_EQUAL(-1, result);  /* Invalid, too large */
}

/* =================================================================
 * Tests for 5-byte and 6-byte utf8_mbtowc() decoding
 * ================================================================= */

static void test_utf8_mbtowc_five_byte_sequence(void)
{
    int wc;
    /* 5-byte UTF-8 sequence for U+200000: F8 88 80 80 80 */
    const char utf8[] = {0xF8, 0x88, 0x80, 0x80, 0x80, 0};
    int result = utf8_mbtowc(&wc, utf8, 5);
    TEST_ASSERT_EQUAL(5, result);
    TEST_ASSERT_EQUAL(0x200000, wc);
}

static void test_utf8_mbtowc_six_byte_sequence(void)
{
    int wc;
    /* 6-byte UTF-8 sequence for U+4000000: FC 84 80 80 80 80 */
    const char utf8[] = {0xFC, 0x84, 0x80, 0x80, 0x80, 0x80, 0};
    int result = utf8_mbtowc(&wc, utf8, 6);
    TEST_ASSERT_EQUAL(6, result);
    TEST_ASSERT_EQUAL(0x4000000, wc);
}

static void test_utf8_mbtowc_invalid_fe_byte(void)
{
    int wc;
    /* 0xFE and 0xFF are never valid in UTF-8 */
    const char utf8[] = {0xFE, 0x80, 0x80, 0x80, 0x80, 0x80, 0};
    int result = utf8_mbtowc(&wc, utf8, 6);
    TEST_ASSERT_EQUAL(-1, result);  /* Invalid lead byte */
}

static void test_utf8_mbtowc_truncated_five_byte(void)
{
    int wc;
    /* 5-byte sequence but only 4 bytes available */
    const char utf8[] = {0xF8, 0x88, 0x80, 0x80, 0};
    int result = utf8_mbtowc(&wc, utf8, 4);
    TEST_ASSERT_EQUAL(-1, result);  /* Truncated */
}

static void test_utf8_mbtowc_truncated_six_byte(void)
{
    int wc;
    /* 6-byte sequence but only 5 bytes available */
    const char utf8[] = {0xFC, 0x84, 0x80, 0x80, 0x80, 0};
    int result = utf8_mbtowc(&wc, utf8, 5);
    TEST_ASSERT_EQUAL(-1, result);  /* Truncated */
}

static void test_utf8_mbtowc_invalid_continuation_in_multibyte(void)
{
    int wc;
    /* 3-byte sequence with invalid continuation byte */
    const char utf8[] = {0xE2, 0x82, 0x00, 0};  /* Third byte should be continuation */
    int result = utf8_mbtowc(&wc, utf8, 3);
    TEST_ASSERT_EQUAL(-1, result);  /* Invalid continuation */
}

static void test_utf8_mbtowc_overlong_encoding(void)
{
    int wc;
    /* Overlong encoding of ASCII 'A' (0x41) using 3 bytes - should be rejected */
    /* Valid 3-byte would be for code points >= 0x800, but 0xE0 0x81 0x81 = 0x41 */
    const char utf8[] = {0xE0, 0x81, 0x81, 0};
    int result = utf8_mbtowc(&wc, utf8, 3);
    TEST_ASSERT_EQUAL(-1, result);  /* Overlong encoding rejected */
}

static void test_utf8_mbtowc_null_pwc(void)
{
    /* Test with NULL pwc - should still work, just not store the result */
    const char utf8[] = "A";
    int result = utf8_mbtowc(NULL, utf8, 1);
    TEST_ASSERT_EQUAL(1, result);
}

static void test_utf8_mbtowc_two_byte_null_pwc(void)
{
    /* Test 2-byte sequence with NULL pwc */
    const char utf8[] = {0xC3, 0xA9, 0};
    int result = utf8_mbtowc(NULL, utf8, 2);
    TEST_ASSERT_EQUAL(2, result);
}

static void test_utf8_mbtowc_multibyte_null_pwc(void)
{
    /* Test 4-byte sequence with NULL pwc */
    const char utf8[] = {0xF0, 0x9F, 0x98, 0x80, 0};
    int result = utf8_mbtowc(NULL, utf8, 4);
    TEST_ASSERT_EQUAL(4, result);
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
 * Tests for ISO-8859-2 (8-bit charset with table mapping)
 * ================================================================= */

static void test_charset_find_iso_8859_2(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);
}

static void test_charset_max_iso_8859_2(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_EQUAL(1, charset_max(cs));  /* 8-bit encoding */
}

static void test_charset_mbtowc_iso_8859_2_ascii(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    int result = charset_mbtowc(cs, &wc, "A", 1);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL('A', wc);
}

static void test_charset_mbtowc_iso_8859_2_special_char(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    /* ISO-8859-2 byte 0xA1 maps to U+0104 (Latin capital letter A with ogonek) */
    const char input[] = {0xA1, 0};
    int result = charset_mbtowc(cs, &wc, input, 1);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL(0x0104, wc);
}

static void test_charset_mbtowc_iso_8859_2_null_pwc(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);

    int result = charset_mbtowc(cs, NULL, "X", 1);
    TEST_ASSERT_EQUAL(1, result);
}

static void test_charset_mbtowc_iso_8859_2_null_byte(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    const char input[] = {0x00};
    int result = charset_mbtowc(cs, &wc, input, 1);
    TEST_ASSERT_EQUAL(0, result);  /* Null byte returns 0 */
}

static void test_charset_mbtowc_iso_8859_2_null_string(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    int result = charset_mbtowc(cs, &wc, NULL, 0);
    TEST_ASSERT_EQUAL(0, result);
}

static void test_charset_mbtowc_iso_8859_2_zero_length(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    int result = charset_mbtowc(cs, &wc, "X", 0);
    TEST_ASSERT_EQUAL(0, result);
}

static void test_charset_wctomb_iso_8859_2_ascii(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);

    char buf[2] = {0};
    int result = charset_wctomb(cs, buf, 'A');
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL('A', buf[0]);
}

static void test_charset_wctomb_iso_8859_2_special_char(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);

    char buf[2] = {0};
    /* U+0104 (A with ogonek) should map to 0xA1 in ISO-8859-2 */
    int result = charset_wctomb(cs, buf, 0x0104);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL((char)0xA1, buf[0]);
}

static void test_charset_wctomb_iso_8859_2_null_buffer(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);

    int result = charset_wctomb(cs, NULL, 'A');
    TEST_ASSERT_EQUAL(0, result);
}

static void test_charset_wctomb_iso_8859_2_unmappable_char(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);

    char buf[2] = {0};
    /* U+20AC (Euro sign) is not in ISO-8859-2 */
    int result = charset_wctomb(cs, buf, 0x20AC);
    TEST_ASSERT_EQUAL(-1, result);  /* Not representable */
}

static void test_charset_wctomb_iso_8859_2_too_large(void)
{
    struct charset *cs = charset_find("ISO-8859-2");
    TEST_ASSERT_NOT_NULL(cs);

    char buf[2] = {0};
    /* Code point > 0xFFFF should return -1 */
    int result = charset_wctomb(cs, buf, 0x10000);
    TEST_ASSERT_EQUAL(-1, result);
}

/* =================================================================
 * Tests for charset name aliasing
 * ================================================================= */

static void test_charset_find_ansi_alias(void)
{
    /* ANSI_X3.4-1968 should map to us-ascii */
    struct charset *cs = charset_find("ANSI_X3.4-1968");
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_EQUAL(1, charset_max(cs));  /* ASCII is 1 byte */
}

/* =================================================================
 * Tests for ASCII wctomb edge cases
 * ================================================================= */

static void test_charset_wctomb_ascii_null_buffer(void)
{
    struct charset *cs = charset_find("US-ASCII");
    TEST_ASSERT_NOT_NULL(cs);

    int result = charset_wctomb(cs, NULL, 'A');
    TEST_ASSERT_EQUAL(0, result);
}

static void test_charset_wctomb_ascii_high_bit_invalid(void)
{
    struct charset *cs = charset_find("US-ASCII");
    TEST_ASSERT_NOT_NULL(cs);

    char buf[2] = {0};
    int result = charset_wctomb(cs, buf, 0x80);  /* Outside ASCII range */
    TEST_ASSERT_EQUAL(-1, result);
}

static void test_charset_mbtowc_ascii_null_string(void)
{
    struct charset *cs = charset_find("US-ASCII");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    int result = charset_mbtowc(cs, &wc, NULL, 0);
    TEST_ASSERT_EQUAL(0, result);
}

static void test_charset_mbtowc_ascii_zero_length(void)
{
    struct charset *cs = charset_find("US-ASCII");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    int result = charset_mbtowc(cs, &wc, "X", 0);
    TEST_ASSERT_EQUAL(0, result);
}

static void test_charset_mbtowc_ascii_null_byte(void)
{
    struct charset *cs = charset_find("US-ASCII");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    const char input[] = {0x00};
    int result = charset_mbtowc(cs, &wc, input, 1);
    TEST_ASSERT_EQUAL(0, result);
}

static void test_charset_mbtowc_ascii_null_pwc(void)
{
    struct charset *cs = charset_find("US-ASCII");
    TEST_ASSERT_NOT_NULL(cs);

    int result = charset_mbtowc(cs, NULL, "A", 1);
    TEST_ASSERT_EQUAL(1, result);
}

/* =================================================================
 * Tests for ISO-8859-1 wctomb edge cases
 * ================================================================= */

static void test_charset_wctomb_iso_8859_1_null_buffer(void)
{
    struct charset *cs = charset_find("ISO-8859-1");
    TEST_ASSERT_NOT_NULL(cs);

    int result = charset_wctomb(cs, NULL, 'A');
    TEST_ASSERT_EQUAL(0, result);
}

static void test_charset_wctomb_iso_8859_1_high_bit_valid(void)
{
    struct charset *cs = charset_find("ISO-8859-1");
    TEST_ASSERT_NOT_NULL(cs);

    char buf[2] = {0};
    int result = charset_wctomb(cs, buf, 0xE9);  /* e with acute, valid in ISO-8859-1 */
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL((char)0xE9, buf[0]);
}

static void test_charset_wctomb_iso_8859_1_too_large(void)
{
    struct charset *cs = charset_find("ISO-8859-1");
    TEST_ASSERT_NOT_NULL(cs);

    char buf[2] = {0};
    int result = charset_wctomb(cs, buf, 0x100);  /* Outside ISO-8859-1 range */
    TEST_ASSERT_EQUAL(-1, result);
}

static void test_charset_mbtowc_iso_8859_1_null_string(void)
{
    struct charset *cs = charset_find("ISO-8859-1");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    int result = charset_mbtowc(cs, &wc, NULL, 0);
    TEST_ASSERT_EQUAL(0, result);
}

static void test_charset_mbtowc_iso_8859_1_zero_length(void)
{
    struct charset *cs = charset_find("ISO-8859-1");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    int result = charset_mbtowc(cs, &wc, "X", 0);
    TEST_ASSERT_EQUAL(0, result);
}

static void test_charset_mbtowc_iso_8859_1_null_byte(void)
{
    struct charset *cs = charset_find("ISO-8859-1");
    TEST_ASSERT_NOT_NULL(cs);

    int wc;
    const char input[] = {0x00};
    int result = charset_mbtowc(cs, &wc, input, 1);
    TEST_ASSERT_EQUAL(0, result);
}

static void test_charset_mbtowc_iso_8859_1_null_pwc(void)
{
    struct charset *cs = charset_find("ISO-8859-1");
    TEST_ASSERT_NOT_NULL(cs);

    int result = charset_mbtowc(cs, NULL, "A", 1);
    TEST_ASSERT_EQUAL(1, result);
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

static void test_charset_convert_null_tolen_allowed(void)
{
    /* When tolen is NULL, the function should still work */
    char *output = NULL;

    int result = charset_convert("UTF-8", "US-ASCII", "test", 4, &output, NULL);

    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("test", output);

    free(output);
}

static void test_charset_convert_unknown_target_encoding(void)
{
    char *output = NULL;
    size_t outlen = 0;

    int result = charset_convert("UTF-8", "UNKNOWN-CHARSET", "test", 4, &output, &outlen);

    TEST_ASSERT_EQUAL(-1, result);  /* Unknown encoding */
}

static void test_charset_convert_iso_8859_2_to_utf8(void)
{
    char *output = NULL;
    size_t outlen = 0;

    /* ISO-8859-2 byte 0xA1 (A with ogonek, U+0104) -> UTF-8: 0xC4 0x84 */
    const char iso2[] = {0xA1, 0};

    int result = charset_convert("ISO-8859-2", "UTF-8", iso2, 1, &output, &outlen);

    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL(2, outlen);
    TEST_ASSERT_EQUAL((char)0xC4, output[0]);
    TEST_ASSERT_EQUAL((char)0x84, output[1]);

    free(output);
}

static void test_charset_convert_utf8_to_iso_8859_2(void)
{
    char *output = NULL;
    size_t outlen = 0;

    /* UTF-8 for U+0104 (A with ogonek): 0xC4 0x84 -> ISO-8859-2: 0xA1 */
    const char utf8[] = {0xC4, 0x84, 0};

    int result = charset_convert("UTF-8", "ISO-8859-2", utf8, 2, &output, &outlen);

    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL(1, outlen);
    TEST_ASSERT_EQUAL((char)0xA1, output[0]);

    free(output);
}

static void test_charset_convert_utf8_to_iso_8859_2_unmappable(void)
{
    char *output = NULL;
    size_t outlen = 0;

    /* UTF-8 for Euro sign U+20AC: not in ISO-8859-2 */
    const char utf8[] = {0xE2, 0x82, 0xAC, 0};

    int result = charset_convert("UTF-8", "ISO-8859-2", utf8, 3, &output, &outlen);

    /* Result 1 means approximate conversion (using '?') */
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("?", output);

    free(output);
}

static void test_charset_convert_with_embedded_null(void)
{
    char *output = NULL;
    size_t outlen = 0;

    /* String with embedded null: "a\0b" */
    const char input[] = {'a', '\0', 'b'};

    int result = charset_convert("UTF-8", "US-ASCII", input, 3, &output, &outlen);

    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(3, outlen);
    TEST_ASSERT_EQUAL('a', output[0]);
    TEST_ASSERT_EQUAL('\0', output[1]);
    TEST_ASSERT_EQUAL('b', output[2]);

    free(output);
}

static void test_charset_convert_long_string(void)
{
    char *output = NULL;
    size_t outlen = 0;

    /* Test a longer string to exercise the realloc path */
    const char *input = "This is a longer test string with multiple words to ensure proper handling";
    size_t len = strlen(input);

    int result = charset_convert("UTF-8", "US-ASCII", input, len, &output, &outlen);

    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL(len, outlen);
    TEST_ASSERT_EQUAL_STRING(input, output);

    free(output);
}

static void test_charset_convert_wctomb_returns_zero(void)
{
    char *output = NULL;
    size_t outlen = 0;

    /* Test case where target wctomb fails completely for replacement char
     * This is hard to trigger since '?' is always valid in ASCII
     * Instead we test that the code path handles zero-length output correctly */
    const char input[] = {0};  /* Just null terminator */

    int result = charset_convert("UTF-8", "US-ASCII", input, 0, &output, &outlen);

    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL(0, outlen);

    free(output);
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
    RUN_TEST(test_utf8_mbtowc_five_byte_sequence);
    RUN_TEST(test_utf8_mbtowc_six_byte_sequence);
    RUN_TEST(test_utf8_mbtowc_invalid_fe_byte);
    RUN_TEST(test_utf8_mbtowc_truncated_five_byte);
    RUN_TEST(test_utf8_mbtowc_truncated_six_byte);
    RUN_TEST(test_utf8_mbtowc_invalid_continuation_in_multibyte);
    RUN_TEST(test_utf8_mbtowc_overlong_encoding);
    RUN_TEST(test_utf8_mbtowc_null_pwc);
    RUN_TEST(test_utf8_mbtowc_two_byte_null_pwc);
    RUN_TEST(test_utf8_mbtowc_multibyte_null_pwc);

    /* utf8_wctomb tests */
    RUN_TEST(test_utf8_wctomb_ascii_char);
    RUN_TEST(test_utf8_wctomb_two_byte_char);
    RUN_TEST(test_utf8_wctomb_three_byte_char);
    RUN_TEST(test_utf8_wctomb_four_byte_char);
    RUN_TEST(test_utf8_wctomb_null_buffer);
    RUN_TEST(test_utf8_wctomb_five_byte_char);
    RUN_TEST(test_utf8_wctomb_six_byte_char);
    RUN_TEST(test_utf8_wctomb_invalid_too_large);

    /* charset_mbtowc / charset_wctomb tests */
    RUN_TEST(test_charset_mbtowc_utf8);
    RUN_TEST(test_charset_wctomb_utf8);
    RUN_TEST(test_charset_mbtowc_ascii);
    RUN_TEST(test_charset_mbtowc_ascii_high_bit_invalid);
    RUN_TEST(test_charset_mbtowc_iso_8859_1);

    /* ISO-8859-2 (8-bit table charset) tests */
    RUN_TEST(test_charset_find_iso_8859_2);
    RUN_TEST(test_charset_max_iso_8859_2);
    RUN_TEST(test_charset_mbtowc_iso_8859_2_ascii);
    RUN_TEST(test_charset_mbtowc_iso_8859_2_special_char);
    RUN_TEST(test_charset_mbtowc_iso_8859_2_null_pwc);
    RUN_TEST(test_charset_mbtowc_iso_8859_2_null_byte);
    RUN_TEST(test_charset_mbtowc_iso_8859_2_null_string);
    RUN_TEST(test_charset_mbtowc_iso_8859_2_zero_length);
    RUN_TEST(test_charset_wctomb_iso_8859_2_ascii);
    RUN_TEST(test_charset_wctomb_iso_8859_2_special_char);
    RUN_TEST(test_charset_wctomb_iso_8859_2_null_buffer);
    RUN_TEST(test_charset_wctomb_iso_8859_2_unmappable_char);
    RUN_TEST(test_charset_wctomb_iso_8859_2_too_large);

    /* Charset name aliasing tests */
    RUN_TEST(test_charset_find_ansi_alias);

    /* ASCII edge cases */
    RUN_TEST(test_charset_wctomb_ascii_null_buffer);
    RUN_TEST(test_charset_wctomb_ascii_high_bit_invalid);
    RUN_TEST(test_charset_mbtowc_ascii_null_string);
    RUN_TEST(test_charset_mbtowc_ascii_zero_length);
    RUN_TEST(test_charset_mbtowc_ascii_null_byte);
    RUN_TEST(test_charset_mbtowc_ascii_null_pwc);

    /* ISO-8859-1 edge cases */
    RUN_TEST(test_charset_wctomb_iso_8859_1_null_buffer);
    RUN_TEST(test_charset_wctomb_iso_8859_1_high_bit_valid);
    RUN_TEST(test_charset_wctomb_iso_8859_1_too_large);
    RUN_TEST(test_charset_mbtowc_iso_8859_1_null_string);
    RUN_TEST(test_charset_mbtowc_iso_8859_1_zero_length);
    RUN_TEST(test_charset_mbtowc_iso_8859_1_null_byte);
    RUN_TEST(test_charset_mbtowc_iso_8859_1_null_pwc);

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
    RUN_TEST(test_charset_convert_null_tolen_allowed);
    RUN_TEST(test_charset_convert_unknown_target_encoding);
    RUN_TEST(test_charset_convert_iso_8859_2_to_utf8);
    RUN_TEST(test_charset_convert_utf8_to_iso_8859_2);
    RUN_TEST(test_charset_convert_utf8_to_iso_8859_2_unmappable);
    RUN_TEST(test_charset_convert_with_embedded_null);
    RUN_TEST(test_charset_convert_long_string);
    RUN_TEST(test_charset_convert_wctomb_returns_zero);

    return UNITY_END();
}
