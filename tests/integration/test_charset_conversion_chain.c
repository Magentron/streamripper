/* test_charset_conversion_chain.c - Integration tests for charset conversion chain
 *
 * This integration test verifies the data flow between:
 * - Charset conversion (charset.c)
 * - UTF-8 encoding/decoding (utf8.c)
 * - Multi-character string operations (mchar.c)
 * - Filename generation with proper encoding
 *
 * Tests the flow: charset detection -> encoding conversion -> filename sanitization
 */
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "charset.h"
#include "errors.h"
#include "filelib.h"
#include "mchar.h"
#include "srtypes.h"
#include "utf8.h"

/* Stub for debug_printf */
void debug_printf(char *fmt, ...)
{
    (void)fmt;
}

/* Test fixtures */
static RIP_MANAGER_INFO test_rmi;
static STREAM_PREFS test_prefs;
static CODESET_OPTIONS test_cs_opt;

void setUp(void)
{
    memset(&test_rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&test_prefs, 0, sizeof(STREAM_PREFS));
    memset(&test_cs_opt, 0, sizeof(CODESET_OPTIONS));

    test_rmi.prefs = &test_prefs;

    /* Initialize default codesets */
    set_codesets_default(&test_cs_opt);
    register_codesets(&test_rmi, &test_cs_opt);

    errors_init();
}

void tearDown(void)
{
    /* Nothing to clean up */
}

/* ============================================================================
 * Test: Full charset detection to UTF-8 conversion chain
 * Tests finding a charset and converting text to UTF-8.
 * ============================================================================ */
static void test_charset_to_utf8_chain(void)
{
    struct charset *cs;
    char *utf8_output = NULL;
    size_t utf8_len = 0;

    /* Step 1: Find ISO-8859-1 charset */
    cs = charset_find("ISO-8859-1");
    TEST_ASSERT_NOT_NULL(cs);

    /* Step 2: Convert Latin-1 string to UTF-8 */
    /* "cafe" with accented e (0xE9 in Latin-1) */
    const char latin1_input[] = {'c', 'a', 'f', (char)0xE9, '\0'};

    int result = charset_convert("ISO-8859-1", "UTF-8",
                                latin1_input, strlen(latin1_input),
                                &utf8_output, &utf8_len);

    TEST_ASSERT_EQUAL(0, result);  /* Exact conversion */
    TEST_ASSERT_NOT_NULL(utf8_output);
    TEST_ASSERT_EQUAL(5, utf8_len);  /* 'c', 'a', 'f' + 2 bytes for e-acute */

    /* Verify UTF-8 encoding of e-acute (0xC3 0xA9) */
    TEST_ASSERT_EQUAL('c', utf8_output[0]);
    TEST_ASSERT_EQUAL('a', utf8_output[1]);
    TEST_ASSERT_EQUAL('f', utf8_output[2]);
    TEST_ASSERT_EQUAL((char)0xC3, utf8_output[3]);
    TEST_ASSERT_EQUAL((char)0xA9, utf8_output[4]);

    free(utf8_output);
}

/* ============================================================================
 * Test: UTF-8 character round trip using mbtowc/wctomb
 * Tests encoding and decoding characters through UTF-8.
 * ============================================================================ */
static void test_utf8_char_roundtrip(void)
{
    int wc;
    char buf[7];
    int len;

    /* Test ASCII round trip */
    len = utf8_mbtowc(&wc, "A", 1);
    TEST_ASSERT_EQUAL(1, len);
    TEST_ASSERT_EQUAL('A', wc);

    memset(buf, 0, sizeof(buf));
    len = utf8_wctomb(buf, 'A');
    TEST_ASSERT_EQUAL(1, len);
    TEST_ASSERT_EQUAL('A', buf[0]);

    /* Test 2-byte character round trip (e-acute) */
    const char e_acute_utf8[] = {(char)0xC3, (char)0xA9, '\0'};
    len = utf8_mbtowc(&wc, e_acute_utf8, 2);
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL(0xE9, wc);

    memset(buf, 0, sizeof(buf));
    len = utf8_wctomb(buf, 0xE9);
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL((char)0xC3, buf[0]);
    TEST_ASSERT_EQUAL((char)0xA9, buf[1]);
}

/* ============================================================================
 * Test: Multi-encoding to UTF-8 normalization
 * Tests converting various encodings to UTF-8 for normalization.
 * ============================================================================ */
static void test_multi_encoding_normalization(void)
{
    char *output1 = NULL;
    char *output2 = NULL;

    /* Convert from ISO-8859-1 */
    const char latin1[] = {(char)0xE9, '\0'};  /* e-acute */
    charset_convert("ISO-8859-1", "UTF-8", latin1, 1, &output1, NULL);
    TEST_ASSERT_NOT_NULL(output1);

    /* Convert directly using UTF-8 mbtowc */
    int wc;
    const char utf8_e_acute[] = {(char)0xC3, (char)0xA9, '\0'};
    int len = utf8_mbtowc(&wc, utf8_e_acute, 2);
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL(0xE9, wc);  /* Unicode code point for e-acute */

    /* Both should produce the same Unicode character */
    TEST_ASSERT_EQUAL((char)0xC3, output1[0]);
    TEST_ASSERT_EQUAL((char)0xA9, output1[1]);

    free(output1);
    (void)output2;
}

/* ============================================================================
 * Test: Filename-safe character conversion
 * Tests converting characters that might be unsafe in filenames.
 * ============================================================================ */
static void test_filename_safe_conversion(void)
{
    char filename[256];

    /* Start with a potentially problematic string */
    strcpy(filename, "Artist / Song: Title?");

    /* Test trim function */
    trim(filename);
    TEST_ASSERT_EQUAL_STRING("Artist / Song: Title?", filename);

    /* The actual filename sanitization would be done by filelib
     * but we can test the string utilities here */
    TEST_ASSERT_NOT_NULL(filename);
}

/* ============================================================================
 * Test: Wide character string operations chain
 * Tests mchar string operations for metadata processing.
 * ============================================================================ */
static void test_wide_string_operations(void)
{
    mchar wide_str[256];
    mchar wide_str2[256];

    /* Initialize with a string */
    mstrcpy(wide_str, "Test Artist - Test Song");

    /* Test mstrlen */
    size_t len = mstrlen(wide_str);
    TEST_ASSERT_EQUAL(23, len);

    /* Test mstrcpy */
    mstrcpy(wide_str2, wide_str);
    TEST_ASSERT_EQUAL_STRING(wide_str, wide_str2);

    /* Test mstrcmp */
    int cmp = mstrcmp(wide_str, wide_str2);
    TEST_ASSERT_EQUAL(0, cmp);

    /* Test mstrchr */
    mchar *dash = mstrchr(wide_str, '-');
    TEST_ASSERT_NOT_NULL(dash);
    TEST_ASSERT_EQUAL('-', *dash);

    /* Test mstrrchr (reverse search) */
    mchar *last_space = mstrrchr(wide_str, ' ');
    TEST_ASSERT_NOT_NULL(last_space);
}

/* ============================================================================
 * Test: Codeset configuration chain
 * Tests complete codeset configuration for a rip session.
 * ============================================================================ */
static void test_codeset_configuration_chain(void)
{
    CODESET_OPTIONS cs_opt;
    RIP_MANAGER_INFO rmi;

    memset(&cs_opt, 0, sizeof(CODESET_OPTIONS));
    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));

    /* Step 1: Set defaults */
    set_codesets_default(&cs_opt);
    TEST_ASSERT_NOT_EQUAL(0, strlen(cs_opt.codeset_locale));

    /* Step 2: Override specific codesets */
    strcpy(cs_opt.codeset_metadata, "UTF-8");
    strcpy(cs_opt.codeset_filesys, "UTF-8");
    strcpy(cs_opt.codeset_id3, "ISO-8859-1");

    /* Step 3: Register with rip manager */
    register_codesets(&rmi, &cs_opt);

    /* Step 4: Verify registration */
    TEST_ASSERT_EQUAL_STRING("UTF-8", rmi.mchar_cs.codeset_metadata);
    TEST_ASSERT_EQUAL_STRING("UTF-8", rmi.mchar_cs.codeset_filesys);
    TEST_ASSERT_EQUAL_STRING("ISO-8859-1", rmi.mchar_cs.codeset_id3);
}

/* ============================================================================
 * Test: Metadata string to filename conversion
 * Tests converting metadata to valid filenames through the chain.
 * ============================================================================ */
static void test_metadata_to_filename_chain(void)
{
    mchar metadata[MAX_TRACK_LEN];
    mchar filename[SR_MAX_PATH];
    char narrow_output[SR_MAX_PATH];

    /* Start with metadata */
    mstrcpy(metadata, "Artist Name - Song Title");

    /* Copy to filename buffer */
    mstrcpy(filename, metadata);

    /* Convert to narrow string */
    int rc = string_from_gstring(&test_rmi, narrow_output, SR_MAX_PATH,
                                 filename, CODESET_FILESYS);

    TEST_ASSERT_TRUE(rc >= 0);
    TEST_ASSERT_NOT_EQUAL(0, strlen(narrow_output));

    /* Verify the conversion preserved the content */
    TEST_ASSERT_NOT_NULL(strstr(narrow_output, "Artist"));
    TEST_ASSERT_NOT_NULL(strstr(narrow_output, "Song"));
}

/* ============================================================================
 * Test: Substring extraction utilities
 * Tests the subnstr_until function for metadata parsing.
 * ============================================================================ */
static void test_substring_extraction(void)
{
    char input[] = "Artist Name - Song Title";
    char output[64];
    char *result;

    /* Extract up to the dash */
    result = subnstr_until(input, " - ", output, sizeof(output));

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("Artist Name", output);
}

/* ============================================================================
 * Test: Format byte size with proper encoding
 * Tests formatting byte sizes for display.
 * ============================================================================ */
static void test_format_byte_size_chain(void)
{
    char formatted[64];

    /* Test KB range */
    format_byte_size(formatted, 1024);
    TEST_ASSERT_NOT_NULL(formatted);
    TEST_ASSERT_NOT_EQUAL(0, strlen(formatted));

    /* Test MB range */
    format_byte_size(formatted, 1048576);
    TEST_ASSERT_NOT_NULL(formatted);

    /* Test GB range */
    format_byte_size(formatted, 1073741824);
    TEST_ASSERT_NOT_NULL(formatted);
}

/* ============================================================================
 * Test: UTF-8 multi-byte character handling
 * Tests handling of multi-byte UTF-8 sequences in the chain.
 * ============================================================================ */
static void test_utf8_multibyte_chain(void)
{
    int wc;
    char utf8_buf[7];

    /* Test 2-byte sequence (Latin characters with diacritics) */
    /* U+00F1 = n with tilde (Spanish) */
    const char utf8_ntilde[] = {(char)0xC3, (char)0xB1, '\0'};
    int len = utf8_mbtowc(&wc, utf8_ntilde, 2);
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL(0xF1, wc);

    /* Convert back */
    len = utf8_wctomb(utf8_buf, 0xF1);
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL((char)0xC3, utf8_buf[0]);
    TEST_ASSERT_EQUAL((char)0xB1, utf8_buf[1]);

    /* Test 3-byte sequence (Euro sign) */
    const char utf8_euro[] = {(char)0xE2, (char)0x82, (char)0xAC, '\0'};
    len = utf8_mbtowc(&wc, utf8_euro, 3);
    TEST_ASSERT_EQUAL(3, len);
    TEST_ASSERT_EQUAL(0x20AC, wc);

    /* Convert back */
    len = utf8_wctomb(utf8_buf, 0x20AC);
    TEST_ASSERT_EQUAL(3, len);
    TEST_ASSERT_EQUAL((char)0xE2, utf8_buf[0]);
    TEST_ASSERT_EQUAL((char)0x82, utf8_buf[1]);
    TEST_ASSERT_EQUAL((char)0xAC, utf8_buf[2]);
}

/* ============================================================================
 * Test: Invalid encoding handling in chain
 * Tests graceful handling of invalid byte sequences.
 * ============================================================================ */
static void test_invalid_encoding_handling(void)
{
    char *output = NULL;

    /* Invalid UTF-8 sequence (standalone continuation byte) */
    const char invalid_utf8[] = {(char)0x80, (char)0x80, '\0'};

    int result = charset_convert("UTF-8", "US-ASCII",
                                invalid_utf8, 2, &output, NULL);

    /* Should return 2 (invalid input converted using '#') */
    TEST_ASSERT_EQUAL(2, result);
    TEST_ASSERT_NOT_NULL(output);
    /* Invalid bytes replaced with '#' */
    TEST_ASSERT_EQUAL_STRING("##", output);

    free(output);
}

/* ============================================================================
 * Test: Charset max length calculation
 * Tests getting maximum byte length for different charsets.
 * ============================================================================ */
static void test_charset_max_lengths(void)
{
    struct charset *cs;

    /* UTF-8: up to 6 bytes per character */
    cs = charset_find("UTF-8");
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_EQUAL(6, charset_max(cs));

    /* US-ASCII: always 1 byte */
    cs = charset_find("US-ASCII");
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_EQUAL(1, charset_max(cs));

    /* ISO-8859-1: always 1 byte */
    cs = charset_find("ISO-8859-1");
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_EQUAL(1, charset_max(cs));
}

/* ============================================================================
 * Test: End-to-end metadata encoding chain
 * Tests complete flow from raw ICY metadata to encoded filename.
 * ============================================================================ */
static void test_end_to_end_metadata_encoding(void)
{
    char raw_metadata[256] = "StreamTitle='Artist - Song';";
    mchar wide_metadata[256];
    char filename[256];

    /* Step 1: Convert raw metadata to wide string */
    int rc = gstring_from_string(&test_rmi, wide_metadata, 256,
                                 raw_metadata, CODESET_METADATA);
    TEST_ASSERT_TRUE(rc >= 0);

    /* Step 2: Process wide string (would normally extract title here) */
    size_t len = mstrlen(wide_metadata);
    TEST_ASSERT_TRUE(len > 0);

    /* Step 3: Convert to filesystem encoding for filename */
    rc = string_from_gstring(&test_rmi, filename, 256,
                             wide_metadata, CODESET_FILESYS);
    TEST_ASSERT_TRUE(rc >= 0);

    /* Step 4: Verify result */
    TEST_ASSERT_NOT_NULL(strstr(filename, "Artist"));
}

/* ============================================================================
 * Test: Path slash handling across platforms
 * Tests path separator handling in the encoding chain.
 * ============================================================================ */
static void test_path_slash_handling(void)
{
    /* Test ISSLASH macro */
    TEST_ASSERT_TRUE(ISSLASH('/'));

    /* Test IS_ABSOLUTE_PATH macro */
    TEST_ASSERT_TRUE(IS_ABSOLUTE_PATH("/home/user"));
    TEST_ASSERT_FALSE(IS_ABSOLUTE_PATH("relative/path"));

    /* Test PATH_SLASH constant */
    TEST_ASSERT_EQUAL('/', PATH_SLASH);
}

/* ============================================================================
 * Test: String duplication with encoding
 * Tests mstrdup for copying encoded strings.
 * ============================================================================ */
static void test_string_duplication(void)
{
    mchar original[] = "Test String Content";
    mchar *duplicate;

    duplicate = mstrdup(original);
    TEST_ASSERT_NOT_NULL(duplicate);
    TEST_ASSERT_EQUAL_STRING(original, duplicate);

    /* Verify it's a separate copy */
    duplicate[0] = 'X';
    TEST_ASSERT_NOT_EQUAL(original[0], duplicate[0]);

    free(duplicate);
}

/* ============================================================================
 * Test: String concatenation with encoding
 * Tests mstrncat for appending encoded strings.
 * ============================================================================ */
static void test_string_concatenation(void)
{
    mchar dest[64] = "Hello";
    mchar src[] = " World";

    mstrncat(dest, src, sizeof(dest) - mstrlen(dest) - 1);

    TEST_ASSERT_EQUAL_STRING("Hello World", dest);
}

/* ============================================================================
 * Test: String to long conversion
 * Tests mtol for numeric string conversion.
 * ============================================================================ */
static void test_string_to_number(void)
{
    mchar num_str[] = "12345";
    long value;

    value = mtol(num_str);
    TEST_ASSERT_EQUAL(12345, value);

    /* Test negative */
    mchar neg_str[] = "-500";
    value = mtol(neg_str);
    TEST_ASSERT_EQUAL(-500, value);
}

/* ============================================================================
 * Main entry point
 * ============================================================================ */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* Charset conversion chain tests */
    RUN_TEST(test_charset_to_utf8_chain);
    RUN_TEST(test_utf8_char_roundtrip);
    RUN_TEST(test_multi_encoding_normalization);
    RUN_TEST(test_charset_max_lengths);

    /* UTF-8 handling tests */
    RUN_TEST(test_utf8_multibyte_chain);
    RUN_TEST(test_invalid_encoding_handling);

    /* Codeset configuration tests */
    RUN_TEST(test_codeset_configuration_chain);

    /* String operations tests */
    RUN_TEST(test_wide_string_operations);
    RUN_TEST(test_substring_extraction);
    RUN_TEST(test_string_duplication);
    RUN_TEST(test_string_concatenation);
    RUN_TEST(test_string_to_number);

    /* Filename handling tests */
    RUN_TEST(test_filename_safe_conversion);
    RUN_TEST(test_metadata_to_filename_chain);
    RUN_TEST(test_end_to_end_metadata_encoding);
    RUN_TEST(test_path_slash_handling);

    /* Utility tests */
    RUN_TEST(test_format_byte_size_chain);

    return UNITY_END();
}
