/* test_mchar.c - Unit tests for lib/mchar.c
 *
 * Tests wide character/multibyte string functions and general string
 * manipulation utilities used for metadata handling.
 */
#include <string.h>
#include <stdlib.h>
#include <unity.h>

#include "mchar.h"
#include "srtypes.h"

/* Include mocks for dependencies */
#include "Mockdebug.h"


void setUp(void)
{
    Mockdebug_Init();
}

void tearDown(void)
{
    Mockdebug_Verify();
}


/*
 * =========================================================================
 * Tests for subnstr_until()
 * =========================================================================
 */

static void test_subnstr_until_basic(void)
{
    char result[64];
    char *ret;

    ret = subnstr_until("hello world", " ", result, sizeof(result));

    TEST_ASSERT_EQUAL_PTR(result, ret);
    TEST_ASSERT_EQUAL_STRING("hello", result);
}

static void test_subnstr_until_no_match(void)
{
    char result[64];

    subnstr_until("helloworld", " ", result, sizeof(result));

    /* When delimiter not found, should copy up to maxlen-1 */
    TEST_ASSERT_EQUAL_STRING("helloworld", result);
}

static void test_subnstr_until_at_start(void)
{
    char result[64];

    subnstr_until(" hello", " ", result, sizeof(result));

    /* Empty string before delimiter */
    TEST_ASSERT_EQUAL_STRING("", result);
}

static void test_subnstr_until_maxlen_limit(void)
{
    char result[6]; /* Only 5 chars + null */

    subnstr_until("hello world", " ", result, sizeof(result));

    /* Should truncate at maxlen-1 */
    TEST_ASSERT_EQUAL_STRING("hello", result);
}

static void test_subnstr_until_maxlen_truncates(void)
{
    char result[4]; /* Only 3 chars + null */

    subnstr_until("hello world", " ", result, sizeof(result));

    /* Should truncate before finding delimiter */
    TEST_ASSERT_EQUAL_STRING("hel", result);
}

static void test_subnstr_until_multi_char_delimiter(void)
{
    char result[64];

    subnstr_until("hello::world", "::", result, sizeof(result));

    TEST_ASSERT_EQUAL_STRING("hello", result);
}


/*
 * =========================================================================
 * Tests for left_str()
 * =========================================================================
 */

static void test_left_str_truncate(void)
{
    char str[] = "hello world";

    left_str(str, 5);

    TEST_ASSERT_EQUAL_STRING("hello", str);
}

static void test_left_str_no_truncate(void)
{
    char str[] = "hello";

    char *ret = left_str(str, 10);

    TEST_ASSERT_EQUAL_PTR(str, ret);
    TEST_ASSERT_EQUAL_STRING("hello", str);
}

static void test_left_str_exact_length(void)
{
    char str[] = "hello";

    left_str(str, 5);

    TEST_ASSERT_EQUAL_STRING("hello", str);
}

static void test_left_str_zero_length(void)
{
    char str[] = "hello";

    left_str(str, 0);

    TEST_ASSERT_EQUAL_STRING("", str);
}


/*
 * =========================================================================
 * Tests for format_byte_size()
 * =========================================================================
 */

static void test_format_byte_size_bytes(void)
{
    char str[32];

    format_byte_size(str, 500);

    TEST_ASSERT_EQUAL_STRING("500b", str);
}

static void test_format_byte_size_kilobytes(void)
{
    char str[32];

    format_byte_size(str, 2048);

    TEST_ASSERT_EQUAL_STRING("2kb", str);
}

static void test_format_byte_size_megabytes(void)
{
    char str[32];

    format_byte_size(str, 1048576); /* Exactly 1 MB */

    TEST_ASSERT_EQUAL_STRING("1.00M", str);
}

static void test_format_byte_size_megabytes_fraction(void)
{
    char str[32];

    format_byte_size(str, 1572864); /* 1.5 MB */

    TEST_ASSERT_EQUAL_STRING("1.50M", str);
}

static void test_format_byte_size_just_under_kb(void)
{
    char str[32];

    format_byte_size(str, 1023);

    TEST_ASSERT_EQUAL_STRING("1023b", str);
}

static void test_format_byte_size_just_under_mb(void)
{
    char str[32];

    format_byte_size(str, 1048575); /* 1 MB - 1 byte */

    TEST_ASSERT_EQUAL_STRING("1023kb", str);
}

static void test_format_byte_size_zero(void)
{
    char str[32];

    format_byte_size(str, 0);

    TEST_ASSERT_EQUAL_STRING("0b", str);
}


/*
 * =========================================================================
 * Tests for trim()
 * =========================================================================
 */

static void test_trim_leading_spaces(void)
{
    char str[] = "   hello";

    trim(str);

    TEST_ASSERT_EQUAL_STRING("hello", str);
}

static void test_trim_trailing_spaces(void)
{
    char str[] = "hello   ";

    trim(str);

    TEST_ASSERT_EQUAL_STRING("hello", str);
}

static void test_trim_both_sides(void)
{
    char str[] = "  hello  ";

    trim(str);

    TEST_ASSERT_EQUAL_STRING("hello", str);
}

static void test_trim_no_whitespace(void)
{
    char str[] = "hello";

    trim(str);

    TEST_ASSERT_EQUAL_STRING("hello", str);
}

static void test_trim_all_whitespace(void)
{
    char str[] = "     ";

    trim(str);

    TEST_ASSERT_EQUAL_STRING("", str);
}

static void test_trim_empty_string(void)
{
    char str[] = "";

    trim(str);

    TEST_ASSERT_EQUAL_STRING("", str);
}

static void test_trim_tabs_and_newlines(void)
{
    char str[] = "\t\n hello \t\n";

    trim(str);

    TEST_ASSERT_EQUAL_STRING("hello", str);
}

static void test_trim_internal_spaces_preserved(void)
{
    char str[] = "  hello   world  ";

    trim(str);

    TEST_ASSERT_EQUAL_STRING("hello   world", str);
}


/*
 * =========================================================================
 * Tests for sr_strncpy()
 * =========================================================================
 */

static void test_sr_strncpy_basic(void)
{
    char dst[32];

    sr_strncpy(dst, "hello", sizeof(dst));

    TEST_ASSERT_EQUAL_STRING("hello", dst);
}

static void test_sr_strncpy_truncates(void)
{
    char dst[4];

    sr_strncpy(dst, "hello", sizeof(dst));

    /* Should copy only 3 chars + null */
    TEST_ASSERT_EQUAL_STRING("hel", dst);
}

static void test_sr_strncpy_exact_fit(void)
{
    char dst[6];

    sr_strncpy(dst, "hello", sizeof(dst));

    TEST_ASSERT_EQUAL_STRING("hello", dst);
}

static void test_sr_strncpy_empty_src(void)
{
    char dst[32];
    memset(dst, 'X', sizeof(dst));

    sr_strncpy(dst, "", sizeof(dst));

    TEST_ASSERT_EQUAL_STRING("", dst);
}

static void test_sr_strncpy_size_one(void)
{
    char dst[1];

    sr_strncpy(dst, "hello", sizeof(dst));

    /* Only null terminator fits */
    TEST_ASSERT_EQUAL_STRING("", dst);
}


/*
 * =========================================================================
 * Tests for mchar string functions (mstrdup, mstrcpy, etc.)
 * =========================================================================
 */

static void test_mstrdup_basic(void)
{
    mchar src[] = "hello";
    mchar *result;

    result = mstrdup(src);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("hello", result);
    TEST_ASSERT_NOT_EQUAL(src, result); /* Should be different memory */

    free(result);
}

static void test_mstrdup_empty(void)
{
    mchar src[] = "";
    mchar *result;

    result = mstrdup(src);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("", result);

    free(result);
}

static void test_mstrcpy_basic(void)
{
    mchar src[] = "hello";
    mchar dst[32];

    mstrcpy(dst, src);

    TEST_ASSERT_EQUAL_STRING("hello", dst);
}

static void test_mstrcpy_empty(void)
{
    mchar src[] = "";
    mchar dst[32];
    memset(dst, 'X', sizeof(dst));

    mstrcpy(dst, src);

    TEST_ASSERT_EQUAL_STRING("", dst);
}

static void test_mstrlen_basic(void)
{
    mchar str[] = "hello";

    TEST_ASSERT_EQUAL(5, mstrlen(str));
}

static void test_mstrlen_empty(void)
{
    mchar str[] = "";

    TEST_ASSERT_EQUAL(0, mstrlen(str));
}

static void test_mstrncpy_basic(void)
{
    mchar src[] = "hello world";
    mchar dst[32];

    mstrncpy(dst, src, sizeof(dst));

    TEST_ASSERT_EQUAL_STRING("hello world", dst);
}

static void test_mstrncpy_truncates(void)
{
    mchar src[] = "hello world";
    mchar dst[6];

    mstrncpy(dst, src, sizeof(dst));

    /* Should copy 5 chars + null */
    TEST_ASSERT_EQUAL_STRING("hello", dst);
}

static void test_mstrncat_basic(void)
{
    mchar dst[32] = "hello";
    mchar src[] = " world";

    mstrncat(dst, src, sizeof(dst) - strlen(dst) - 1);

    TEST_ASSERT_EQUAL_STRING("hello world", dst);
}

static void test_mstrncat_truncates(void)
{
    mchar dst[10] = "hello";
    mchar src[] = " world";

    /* Only room for 4 more chars + existing content */
    mstrncat(dst, src, 4);

    TEST_ASSERT_EQUAL_STRING("hello wor", dst);
}

static void test_mstrcmp_equal(void)
{
    mchar str1[] = "hello";
    mchar str2[] = "hello";

    TEST_ASSERT_EQUAL(0, mstrcmp(str1, str2));
}

static void test_mstrcmp_less_than(void)
{
    mchar str1[] = "abc";
    mchar str2[] = "abd";

    TEST_ASSERT_TRUE(mstrcmp(str1, str2) < 0);
}

static void test_mstrcmp_greater_than(void)
{
    mchar str1[] = "abd";
    mchar str2[] = "abc";

    TEST_ASSERT_TRUE(mstrcmp(str1, str2) > 0);
}

static void test_mstrcmp_empty_strings(void)
{
    mchar str1[] = "";
    mchar str2[] = "";

    TEST_ASSERT_EQUAL(0, mstrcmp(str1, str2));
}

static void test_mtol_basic(void)
{
    mchar str[] = "12345";

    TEST_ASSERT_EQUAL(12345, mtol(str));
}

static void test_mtol_negative(void)
{
    mchar str[] = "-12345";

    TEST_ASSERT_EQUAL(-12345, mtol(str));
}

static void test_mtol_hex(void)
{
    mchar str[] = "0xFF";

    TEST_ASSERT_EQUAL(255, mtol(str));
}

static void test_mtol_zero(void)
{
    mchar str[] = "0";

    TEST_ASSERT_EQUAL(0, mtol(str));
}

static void test_mtol_with_trailing_text(void)
{
    mchar str[] = "123abc";

    TEST_ASSERT_EQUAL(123, mtol(str));
}


/*
 * =========================================================================
 * Tests for msnprintf()
 * =========================================================================
 */

static void test_msnprintf_basic(void)
{
    mchar dst[32];
    int ret;

    ret = msnprintf(dst, sizeof(dst), "hello %s", "world");

    TEST_ASSERT_EQUAL_STRING("hello world", dst);
    TEST_ASSERT_EQUAL(11, ret);
}

static void test_msnprintf_integer(void)
{
    mchar dst[32];

    msnprintf(dst, sizeof(dst), "value: %d", 42);

    TEST_ASSERT_EQUAL_STRING("value: 42", dst);
}

static void test_msnprintf_truncates(void)
{
    mchar dst[8];
    int ret;

    ret = msnprintf(dst, sizeof(dst), "hello %s", "world");

    /* Should be truncated and null-terminated */
    TEST_ASSERT_EQUAL_STRING("hello w", dst);
    /* Return value indicates what would have been written */
    TEST_ASSERT_EQUAL(11, ret);
}

static void test_msnprintf_empty_format(void)
{
    mchar dst[32];

    msnprintf(dst, sizeof(dst), "");

    TEST_ASSERT_EQUAL_STRING("", dst);
}


/*
 * =========================================================================
 * Tests for mstrchr() and mstrrchr()
 * =========================================================================
 */

static void test_mstrchr_found(void)
{
    mchar str[] = "hello world";
    mchar *result;

    result = mstrchr(str, 'o');

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("o world", result);
}

static void test_mstrchr_not_found(void)
{
    mchar str[] = "hello world";
    mchar *result;

    result = mstrchr(str, 'z');

    TEST_ASSERT_NULL(result);
}

static void test_mstrchr_first_char(void)
{
    mchar str[] = "hello";
    mchar *result;

    result = mstrchr(str, 'h');

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_PTR(str, result);
}

static void test_mstrrchr_found(void)
{
    mchar str[] = "hello world";
    mchar *result;

    result = mstrrchr(str, 'o');

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("orld", result);
}

static void test_mstrrchr_not_found(void)
{
    mchar str[] = "hello world";
    mchar *result;

    result = mstrrchr(str, 'z');

    TEST_ASSERT_NULL(result);
}

static void test_mstrrchr_last_char(void)
{
    mchar str[] = "hello";
    mchar *result;

    result = mstrrchr(str, 'o');

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("o", result);
}


/*
 * =========================================================================
 * Main test runner
 * =========================================================================
 */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* subnstr_until tests */
    RUN_TEST(test_subnstr_until_basic);
    RUN_TEST(test_subnstr_until_no_match);
    RUN_TEST(test_subnstr_until_at_start);
    RUN_TEST(test_subnstr_until_maxlen_limit);
    RUN_TEST(test_subnstr_until_maxlen_truncates);
    RUN_TEST(test_subnstr_until_multi_char_delimiter);

    /* left_str tests */
    RUN_TEST(test_left_str_truncate);
    RUN_TEST(test_left_str_no_truncate);
    RUN_TEST(test_left_str_exact_length);
    RUN_TEST(test_left_str_zero_length);

    /* format_byte_size tests */
    RUN_TEST(test_format_byte_size_bytes);
    RUN_TEST(test_format_byte_size_kilobytes);
    RUN_TEST(test_format_byte_size_megabytes);
    RUN_TEST(test_format_byte_size_megabytes_fraction);
    RUN_TEST(test_format_byte_size_just_under_kb);
    RUN_TEST(test_format_byte_size_just_under_mb);
    RUN_TEST(test_format_byte_size_zero);

    /* trim tests */
    RUN_TEST(test_trim_leading_spaces);
    RUN_TEST(test_trim_trailing_spaces);
    RUN_TEST(test_trim_both_sides);
    RUN_TEST(test_trim_no_whitespace);
    RUN_TEST(test_trim_all_whitespace);
    RUN_TEST(test_trim_empty_string);
    RUN_TEST(test_trim_tabs_and_newlines);
    RUN_TEST(test_trim_internal_spaces_preserved);

    /* sr_strncpy tests */
    RUN_TEST(test_sr_strncpy_basic);
    RUN_TEST(test_sr_strncpy_truncates);
    RUN_TEST(test_sr_strncpy_exact_fit);
    RUN_TEST(test_sr_strncpy_empty_src);
    RUN_TEST(test_sr_strncpy_size_one);

    /* mchar string function tests */
    RUN_TEST(test_mstrdup_basic);
    RUN_TEST(test_mstrdup_empty);
    RUN_TEST(test_mstrcpy_basic);
    RUN_TEST(test_mstrcpy_empty);
    RUN_TEST(test_mstrlen_basic);
    RUN_TEST(test_mstrlen_empty);
    RUN_TEST(test_mstrncpy_basic);
    RUN_TEST(test_mstrncpy_truncates);
    RUN_TEST(test_mstrncat_basic);
    RUN_TEST(test_mstrncat_truncates);
    RUN_TEST(test_mstrcmp_equal);
    RUN_TEST(test_mstrcmp_less_than);
    RUN_TEST(test_mstrcmp_greater_than);
    RUN_TEST(test_mstrcmp_empty_strings);
    RUN_TEST(test_mtol_basic);
    RUN_TEST(test_mtol_negative);
    RUN_TEST(test_mtol_hex);
    RUN_TEST(test_mtol_zero);
    RUN_TEST(test_mtol_with_trailing_text);

    /* msnprintf tests */
    RUN_TEST(test_msnprintf_basic);
    RUN_TEST(test_msnprintf_integer);
    RUN_TEST(test_msnprintf_truncates);
    RUN_TEST(test_msnprintf_empty_format);

    /* mstrchr/mstrrchr tests */
    RUN_TEST(test_mstrchr_found);
    RUN_TEST(test_mstrchr_not_found);
    RUN_TEST(test_mstrchr_first_char);
    RUN_TEST(test_mstrrchr_found);
    RUN_TEST(test_mstrrchr_not_found);
    RUN_TEST(test_mstrrchr_last_char);

    return UNITY_END();
}
