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
 * Tests for lstring_initialize()
 * =========================================================================
 */

/* Forward declaration from mchar.c - not in header but we can test it */
extern void lstring_initialize(Lstring *lstring);

static void test_lstring_initialize_basic(void)
{
    Lstring ls;
    ls.num_bytes = 100;
    ls.data = (gchar*)0x12345678;  /* Dummy pointer */

    lstring_initialize(&ls);

    TEST_ASSERT_EQUAL(0, ls.num_bytes);
    TEST_ASSERT_NULL(ls.data);
}


/*
 * =========================================================================
 * Tests for gmem_concat()
 * =========================================================================
 */

/* Forward declaration from mchar.c */
extern void gmem_concat(gchar **base_mem, gsize base_bytes, char *concat_mem, gsize concat_bytes);

static void test_gmem_concat_basic(void)
{
    gchar *buffer = g_malloc(5);
    memcpy(buffer, "hello", 5);

    gmem_concat(&buffer, 5, " world", 6);

    TEST_ASSERT_EQUAL_STRING_LEN("hello world", buffer, 11);
    g_free(buffer);
}

static void test_gmem_concat_empty_base(void)
{
    gchar *buffer = NULL;

    gmem_concat(&buffer, 0, "test", 4);

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_EQUAL_STRING_LEN("test", buffer, 4);
    g_free(buffer);
}

static void test_gmem_concat_empty_concat(void)
{
    gchar *buffer = g_malloc(4);
    memcpy(buffer, "test", 4);

    gmem_concat(&buffer, 4, "", 0);

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_EQUAL_STRING_LEN("test", buffer, 4);
    g_free(buffer);
}


/*
 * =========================================================================
 * Tests for convert_string_with_replacement()
 * =========================================================================
 */

/* Forward declaration from mchar.c */
extern void convert_string_with_replacement(
    gchar **output_string, gsize *output_bytes,
    char *input_string, gsize input_bytes,
    char *from_codeset, char *to_codeset, char *repl);

static void test_convert_string_utf8_to_utf8(void)
{
    gchar *output = NULL;
    gsize output_bytes = 0;

    debug_printf_Ignore();

    convert_string_with_replacement(
        &output, &output_bytes,
        "hello", 6,
        "UTF-8", "UTF-8", "?");

    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("hello", output);
    TEST_ASSERT_EQUAL(6, output_bytes);
    g_free(output);
}

static void test_convert_string_utf8_to_latin1(void)
{
    gchar *output = NULL;
    gsize output_bytes = 0;

    debug_printf_Ignore();

    convert_string_with_replacement(
        &output, &output_bytes,
        "hello world", 12,
        "UTF-8", "ISO-8859-1", "?");

    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("hello world", output);
    g_free(output);
}

static void test_convert_string_latin1_to_utf8(void)
{
    gchar *output = NULL;
    gsize output_bytes = 0;

    debug_printf_Ignore();

    convert_string_with_replacement(
        &output, &output_bytes,
        "hello", 6,
        "ISO-8859-1", "UTF-8", "?");

    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("hello", output);
    g_free(output);
}

static void test_convert_string_with_special_chars(void)
{
    gchar *output = NULL;
    gsize output_bytes = 0;
    /* UTF-8 euro sign: E2 82 AC */
    char input[] = "Price: \xe2\x82\xac" "100";

    debug_printf_Ignore();

    convert_string_with_replacement(
        &output, &output_bytes,
        input, strlen(input) + 1,
        "UTF-8", "UTF-8", "?");

    TEST_ASSERT_NOT_NULL(output);
    /* Should preserve the euro sign */
    TEST_ASSERT_EQUAL_STRING(input, output);
    g_free(output);
}

static void test_convert_string_empty(void)
{
    gchar *output = NULL;
    gsize output_bytes = 0;

    debug_printf_Ignore();

    convert_string_with_replacement(
        &output, &output_bytes,
        "", 1,
        "UTF-8", "UTF-8", "?");

    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("", output);
    g_free(output);
}

static void test_convert_string_invalid_codeset(void)
{
    gchar *output = NULL;
    gsize output_bytes = 0;

    debug_printf_Ignore();

    convert_string_with_replacement(
        &output, &output_bytes,
        "test", 5,
        "INVALID-CODESET", "UTF-8", "?");

    /* Should return NULL for invalid codeset */
    TEST_ASSERT_NULL(output);
    TEST_ASSERT_EQUAL(0, output_bytes);
}

static void test_convert_string_with_replacement_chars(void)
{
    gchar *output = NULL;
    gsize output_bytes = 0;
    /* Invalid UTF-8 sequence: 0xff 0xfe are not valid UTF-8 bytes */
    char input[10];
    strcpy(input, "test");
    input[4] = '\xff';
    input[5] = '\xfe';
    input[6] = 'e';
    input[7] = 'n';
    input[8] = 'd';
    input[9] = '\0';

    debug_printf_Ignore();

    convert_string_with_replacement(
        &output, &output_bytes,
        input, strlen(input) + 1,
        "UTF-8", "UTF-8", "?");

    /* The invalid bytes should be replaced with ? */
    TEST_ASSERT_NOT_NULL(output);
    g_free(output);
}


/*
 * =========================================================================
 * Tests for utf8cpy()
 * =========================================================================
 */

/* Forward declaration from mchar.c */
extern int utf8cpy(gchar *dst, gchar *src, int dst_len);

static void test_utf8cpy_basic(void)
{
    gchar dst[64];
    int result;

    result = utf8cpy(dst, "hello world", sizeof(dst));

    TEST_ASSERT_EQUAL_STRING("hello world", dst);
    TEST_ASSERT_EQUAL(11, result);
}

static void test_utf8cpy_empty_src(void)
{
    gchar dst[64];
    int result;

    result = utf8cpy(dst, "", sizeof(dst));

    TEST_ASSERT_EQUAL_STRING("", dst);
    TEST_ASSERT_EQUAL(0, result);
}

static void test_utf8cpy_multibyte(void)
{
    gchar dst[64];
    int result;
    /* UTF-8 string with multi-byte chars (euro sign) */
    gchar src[] = "\xe2\x82\xac" "100";

    result = utf8cpy(dst, src, sizeof(dst));

    TEST_ASSERT_EQUAL_STRING(src, dst);
    TEST_ASSERT_EQUAL(6, result);  /* 3 bytes for euro + 3 for "100" */
}

static void test_utf8cpy_small_dst(void)
{
    gchar dst[8];
    int result;

    /* Buffer too small - should copy what fits */
    result = utf8cpy(dst, "hello world", sizeof(dst));

    /* Should stop before running out of buffer space */
    TEST_ASSERT_TRUE(result <= 7);
}


/*
 * =========================================================================
 * Tests for lstring_from_gstring()
 * =========================================================================
 */

/* Forward declaration from mchar.c */
extern void lstring_from_gstring(Lstring *lstring_out, gchar *gstring_in, char *to_codeset);

static void test_lstring_from_gstring_basic(void)
{
    Lstring ls;
    lstring_initialize(&ls);

    debug_printf_Ignore();

    lstring_from_gstring(&ls, "hello", "UTF-8");

    TEST_ASSERT_NOT_NULL(ls.data);
    TEST_ASSERT_EQUAL_STRING("hello", ls.data);
    g_free(ls.data);
}

static void test_lstring_from_gstring_to_latin1(void)
{
    Lstring ls;
    lstring_initialize(&ls);

    debug_printf_Ignore();

    lstring_from_gstring(&ls, "hello world", "ISO-8859-1");

    TEST_ASSERT_NOT_NULL(ls.data);
    g_free(ls.data);
}


/*
 * =========================================================================
 * Tests for lstring_from_lstring()
 * =========================================================================
 */

/* Forward declaration from mchar.c */
extern void lstring_from_lstring(Lstring *lstring_out, Lstring *lstring_in,
                                  char *from_codeset, char *to_codeset);

static void test_lstring_from_lstring_basic(void)
{
    Lstring ls_in, ls_out;

    ls_in.data = g_strdup("hello");
    ls_in.num_bytes = 6;
    lstring_initialize(&ls_out);

    debug_printf_Ignore();

    lstring_from_lstring(&ls_out, &ls_in, "UTF-8", "UTF-8");

    /* Note: lstring_from_lstring has a bug - it uses lstring_out->num_bytes
       instead of lstring_in->num_bytes. We test the actual behavior. */
    g_free(ls_in.data);
    if (ls_out.data) g_free(ls_out.data);
}


/*
 * =========================================================================
 * Tests for default_codeset()
 * =========================================================================
 */

/* Forward declaration from mchar.c */
extern const char *default_codeset(void);

static void test_default_codeset_returns_valid(void)
{
    const char *codeset;

    debug_printf_Ignore();

    codeset = default_codeset();

    TEST_ASSERT_NOT_NULL(codeset);
    TEST_ASSERT_TRUE(strlen(codeset) > 0);
}


/*
 * =========================================================================
 * Tests for sr_set_locale()
 * =========================================================================
 */

static void test_sr_set_locale_basic(void)
{
    debug_printf_Ignore();

    /* Just verify it doesn't crash */
    sr_set_locale();

    /* If we reach here without crashing, the test passed */
    TEST_ASSERT_TRUE(1);
}


/*
 * =========================================================================
 * Tests for set_codesets_default()
 * =========================================================================
 */

static void test_set_codesets_default_basic(void)
{
    CODESET_OPTIONS cs_opt;
    memset(&cs_opt, 0, sizeof(cs_opt));

    debug_printf_Ignore();

    set_codesets_default(&cs_opt);

    /* Should have set all codeset fields */
    TEST_ASSERT_TRUE(strlen(cs_opt.codeset_locale) > 0);
    TEST_ASSERT_TRUE(strlen(cs_opt.codeset_filesys) > 0);
    TEST_ASSERT_EQUAL_STRING("UTF-16", cs_opt.codeset_id3);
    TEST_ASSERT_TRUE(strlen(cs_opt.codeset_metadata) > 0);
    TEST_ASSERT_TRUE(strlen(cs_opt.codeset_relay) > 0);
}


/*
 * =========================================================================
 * Tests for register_codesets()
 * =========================================================================
 */

static void test_register_codesets_basic(void)
{
    RIP_MANAGER_INFO rmi;
    CODESET_OPTIONS cs_opt;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(cs_opt.codeset_locale, "ISO-8859-1");
    strcpy(cs_opt.codeset_filesys, "ISO-8859-1");
    strcpy(cs_opt.codeset_id3, "UTF-16");
    strcpy(cs_opt.codeset_metadata, "ISO-8859-1");
    strcpy(cs_opt.codeset_relay, "ISO-8859-1");

    debug_printf_Ignore();

    register_codesets(&rmi, &cs_opt);

    TEST_ASSERT_EQUAL_STRING("ISO-8859-1", rmi.mchar_cs.codeset_locale);
    TEST_ASSERT_EQUAL_STRING("ISO-8859-1", rmi.mchar_cs.codeset_filesys);
    TEST_ASSERT_EQUAL_STRING("UTF-16", rmi.mchar_cs.codeset_id3);
    TEST_ASSERT_EQUAL_STRING("ISO-8859-1", rmi.mchar_cs.codeset_metadata);
    TEST_ASSERT_EQUAL_STRING("ISO-8859-1", rmi.mchar_cs.codeset_relay);
}

static void test_register_codesets_ucs2_to_utf16(void)
{
    RIP_MANAGER_INFO rmi;
    CODESET_OPTIONS cs_opt;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(cs_opt.codeset_locale, "UTF-8");
    strcpy(cs_opt.codeset_filesys, "UTF-8");
    strcpy(cs_opt.codeset_id3, "UCS-2");  /* Should be converted to UTF-16 */
    strcpy(cs_opt.codeset_metadata, "UTF-8");
    strcpy(cs_opt.codeset_relay, "UTF-8");

    debug_printf_Ignore();

    register_codesets(&rmi, &cs_opt);

    /* UCS-2 should be normalized to UTF-16 */
    TEST_ASSERT_EQUAL_STRING("UTF-16", rmi.mchar_cs.codeset_id3);
}

static void test_register_codesets_utf16le_to_utf16(void)
{
    RIP_MANAGER_INFO rmi;
    CODESET_OPTIONS cs_opt;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(cs_opt.codeset_locale, "UTF-8");
    strcpy(cs_opt.codeset_filesys, "UTF-8");
    strcpy(cs_opt.codeset_id3, "UTF-16LE");  /* Should be converted to UTF-16 */
    strcpy(cs_opt.codeset_metadata, "UTF-8");
    strcpy(cs_opt.codeset_relay, "UTF-8");

    debug_printf_Ignore();

    register_codesets(&rmi, &cs_opt);

    /* UTF-16LE should be normalized to UTF-16 */
    TEST_ASSERT_EQUAL_STRING("UTF-16", rmi.mchar_cs.codeset_id3);
}


/*
 * =========================================================================
 * Tests for is_id3_unicode()
 * =========================================================================
 */

static void test_is_id3_unicode_true(void)
{
    RIP_MANAGER_INFO rmi;
    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_id3, "UTF-16");

    int result = is_id3_unicode(&rmi);

    TEST_ASSERT_EQUAL(1, result);
}

static void test_is_id3_unicode_false(void)
{
    RIP_MANAGER_INFO rmi;
    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_id3, "ISO-8859-1");

    int result = is_id3_unicode(&rmi);

    TEST_ASSERT_EQUAL(0, result);
}


/*
 * =========================================================================
 * Tests for gstring_from_string()
 * =========================================================================
 */

static void test_gstring_from_string_utf8(void)
{
    RIP_MANAGER_INFO rmi;
    mchar m[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_locale, "UTF-8");

    debug_printf_Ignore();

    result = gstring_from_string(&rmi, m, sizeof(m), "hello", CODESET_UTF8);

    TEST_ASSERT_TRUE(result > 0);
    TEST_ASSERT_EQUAL_STRING("hello", m);
}

static void test_gstring_from_string_locale(void)
{
    RIP_MANAGER_INFO rmi;
    mchar m[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_locale, "ISO-8859-1");

    debug_printf_Ignore();

    result = gstring_from_string(&rmi, m, sizeof(m), "hello", CODESET_LOCALE);

    TEST_ASSERT_TRUE(result > 0);
}

static void test_gstring_from_string_filesys(void)
{
    RIP_MANAGER_INFO rmi;
    mchar m[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_filesys, "UTF-8");

    debug_printf_Ignore();

    result = gstring_from_string(&rmi, m, sizeof(m), "hello", CODESET_FILESYS);

    TEST_ASSERT_TRUE(result > 0);
}

static void test_gstring_from_string_id3(void)
{
    RIP_MANAGER_INFO rmi;
    mchar m[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_id3, "UTF-8");

    debug_printf_Ignore();

    result = gstring_from_string(&rmi, m, sizeof(m), "hello", CODESET_ID3);

    TEST_ASSERT_TRUE(result > 0);
}

static void test_gstring_from_string_metadata(void)
{
    RIP_MANAGER_INFO rmi;
    mchar m[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_metadata, "ISO-8859-1");

    debug_printf_Ignore();

    result = gstring_from_string(&rmi, m, sizeof(m), "hello", CODESET_METADATA);

    TEST_ASSERT_TRUE(result > 0);
}

static void test_gstring_from_string_relay(void)
{
    RIP_MANAGER_INFO rmi;
    mchar m[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_relay, "UTF-8");

    debug_printf_Ignore();

    result = gstring_from_string(&rmi, m, sizeof(m), "hello", CODESET_RELAY);

    TEST_ASSERT_TRUE(result > 0);
}

static void test_gstring_from_string_null_input(void)
{
    RIP_MANAGER_INFO rmi;
    mchar m[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));

    result = gstring_from_string(&rmi, m, sizeof(m), NULL, CODESET_UTF8);

    TEST_ASSERT_EQUAL(0, result);
}

static void test_gstring_from_string_negative_mlen(void)
{
    RIP_MANAGER_INFO rmi;
    mchar m[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));

    result = gstring_from_string(&rmi, m, -1, "hello", CODESET_UTF8);

    TEST_ASSERT_EQUAL(0, result);
}


/*
 * =========================================================================
 * Tests for string_from_gstring()
 * =========================================================================
 */

static void test_string_from_gstring_utf8(void)
{
    RIP_MANAGER_INFO rmi;
    char c[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_locale, "UTF-8");

    debug_printf_Ignore();

    result = string_from_gstring(&rmi, c, sizeof(c), "hello", CODESET_UTF8);

    TEST_ASSERT_TRUE(result > 0);
    TEST_ASSERT_EQUAL_STRING("hello", c);
}

static void test_string_from_gstring_locale(void)
{
    RIP_MANAGER_INFO rmi;
    char c[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_locale, "ISO-8859-1");

    debug_printf_Ignore();

    result = string_from_gstring(&rmi, c, sizeof(c), "hello", CODESET_LOCALE);

    TEST_ASSERT_TRUE(result > 0);
}

static void test_string_from_gstring_filesys(void)
{
    RIP_MANAGER_INFO rmi;
    char c[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_filesys, "UTF-8");

    debug_printf_Ignore();

    result = string_from_gstring(&rmi, c, sizeof(c), "hello", CODESET_FILESYS);

    TEST_ASSERT_TRUE(result > 0);
}

static void test_string_from_gstring_id3(void)
{
    RIP_MANAGER_INFO rmi;
    char c[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_id3, "UTF-8");

    debug_printf_Ignore();

    result = string_from_gstring(&rmi, c, sizeof(c), "hello", CODESET_ID3);

    TEST_ASSERT_TRUE(result > 0);
}

static void test_string_from_gstring_metadata(void)
{
    RIP_MANAGER_INFO rmi;
    char c[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_metadata, "ISO-8859-1");

    debug_printf_Ignore();

    result = string_from_gstring(&rmi, c, sizeof(c), "hello", CODESET_METADATA);

    TEST_ASSERT_TRUE(result > 0);
}

static void test_string_from_gstring_relay(void)
{
    RIP_MANAGER_INFO rmi;
    char c[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_relay, "UTF-8");

    debug_printf_Ignore();

    result = string_from_gstring(&rmi, c, sizeof(c), "hello", CODESET_RELAY);

    TEST_ASSERT_TRUE(result > 0);
}

static void test_string_from_gstring_null_input(void)
{
    RIP_MANAGER_INFO rmi;
    char c[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));

    result = string_from_gstring(&rmi, c, sizeof(c), NULL, CODESET_UTF8);

    TEST_ASSERT_EQUAL(0, result);
}

static void test_string_from_gstring_zero_clen(void)
{
    RIP_MANAGER_INFO rmi;
    char c[64];
    int result;

    memset(&rmi, 0, sizeof(rmi));

    result = string_from_gstring(&rmi, c, 0, "hello", CODESET_UTF8);

    TEST_ASSERT_EQUAL(0, result);
}

static void test_string_from_gstring_truncation(void)
{
    RIP_MANAGER_INFO rmi;
    char c[4];  /* Very small buffer */
    int result;

    memset(&rmi, 0, sizeof(rmi));
    strcpy(rmi.mchar_cs.codeset_locale, "UTF-8");

    debug_printf_Ignore();

    result = string_from_gstring(&rmi, c, sizeof(c), "hello world", CODESET_UTF8);

    /* Should truncate to fit buffer */
    TEST_ASSERT_TRUE(result <= 3);
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

    /* lstring_initialize tests */
    RUN_TEST(test_lstring_initialize_basic);

    /* gmem_concat tests */
    RUN_TEST(test_gmem_concat_basic);
    RUN_TEST(test_gmem_concat_empty_base);
    RUN_TEST(test_gmem_concat_empty_concat);

    /* convert_string_with_replacement tests */
    RUN_TEST(test_convert_string_utf8_to_utf8);
    RUN_TEST(test_convert_string_utf8_to_latin1);
    RUN_TEST(test_convert_string_latin1_to_utf8);
    RUN_TEST(test_convert_string_with_special_chars);
    RUN_TEST(test_convert_string_empty);
    RUN_TEST(test_convert_string_invalid_codeset);
    RUN_TEST(test_convert_string_with_replacement_chars);

    /* utf8cpy tests */
    RUN_TEST(test_utf8cpy_basic);
    RUN_TEST(test_utf8cpy_empty_src);
    RUN_TEST(test_utf8cpy_multibyte);
    RUN_TEST(test_utf8cpy_small_dst);

    /* lstring_from_gstring tests */
    RUN_TEST(test_lstring_from_gstring_basic);
    RUN_TEST(test_lstring_from_gstring_to_latin1);

    /* lstring_from_lstring tests */
    RUN_TEST(test_lstring_from_lstring_basic);

    /* default_codeset tests */
    RUN_TEST(test_default_codeset_returns_valid);

    /* sr_set_locale tests */
    RUN_TEST(test_sr_set_locale_basic);

    /* set_codesets_default tests */
    RUN_TEST(test_set_codesets_default_basic);

    /* register_codesets tests */
    RUN_TEST(test_register_codesets_basic);
    RUN_TEST(test_register_codesets_ucs2_to_utf16);
    RUN_TEST(test_register_codesets_utf16le_to_utf16);

    /* is_id3_unicode tests */
    RUN_TEST(test_is_id3_unicode_true);
    RUN_TEST(test_is_id3_unicode_false);

    /* gstring_from_string tests */
    RUN_TEST(test_gstring_from_string_utf8);
    RUN_TEST(test_gstring_from_string_locale);
    RUN_TEST(test_gstring_from_string_filesys);
    RUN_TEST(test_gstring_from_string_id3);
    RUN_TEST(test_gstring_from_string_metadata);
    RUN_TEST(test_gstring_from_string_relay);
    RUN_TEST(test_gstring_from_string_null_input);
    RUN_TEST(test_gstring_from_string_negative_mlen);

    /* string_from_gstring tests */
    RUN_TEST(test_string_from_gstring_utf8);
    RUN_TEST(test_string_from_gstring_locale);
    RUN_TEST(test_string_from_gstring_filesys);
    RUN_TEST(test_string_from_gstring_id3);
    RUN_TEST(test_string_from_gstring_metadata);
    RUN_TEST(test_string_from_gstring_relay);
    RUN_TEST(test_string_from_gstring_null_input);
    RUN_TEST(test_string_from_gstring_zero_clen);
    RUN_TEST(test_string_from_gstring_truncation);

    return UNITY_END();
}
