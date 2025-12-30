/* test_utf8.c - Unit tests for lib/utf8.c */
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "utf8.h"

void setUp(void)
{
    /* No special setup needed */
}

void tearDown(void)
{
    /* Nothing to tear down */
}

/* =================================================================
 * Tests for utf8_decode() - Convert UTF-8 to locale charset
 *
 * Note: On non-Windows systems, utf8_decode converts UTF-8 to US-ASCII.
 * Non-ASCII characters are replaced with '?'.
 * ================================================================= */

static void test_utf8_decode_empty_string(void)
{
    char *output = NULL;
    int result = utf8_decode("", &output);

    /* Empty string returns 1 (special case in implementation) */
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("", output);

    free(output);
}

static void test_utf8_decode_ascii_only(void)
{
    char *output = NULL;
    int result = utf8_decode("Hello World", &output);

    /* ASCII-only should convert exactly (return 0) or with ? replacement (return 3) */
    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("Hello World", output);

    free(output);
}

static void test_utf8_decode_with_numbers(void)
{
    char *output = NULL;
    int result = utf8_decode("12345", &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("12345", output);

    free(output);
}

static void test_utf8_decode_special_ascii_chars(void)
{
    char *output = NULL;
    int result = utf8_decode("!@#$%^&*()", &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("!@#$%^&*()", output);

    free(output);
}

static void test_utf8_decode_non_ascii_replaced(void)
{
    char *output = NULL;

    /* UTF-8 for "cafe" with accented e: caf + 0xC3 0xA9 */
    const char utf8_input[] = {'c', 'a', 'f', (char)0xC3, (char)0xA9, 0};

    int result = utf8_decode(utf8_input, &output);

    /* Non-ASCII should be replaced with '?' */
    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    /* The accented e is replaced with '?' */
    TEST_ASSERT_EQUAL_STRING("caf?", output);

    free(output);
}

static void test_utf8_decode_euro_sign(void)
{
    char *output = NULL;

    /* UTF-8 for Euro sign: 0xE2 0x82 0xAC */
    const char utf8_input[] = {(char)0xE2, (char)0x82, (char)0xAC, 0};

    int result = utf8_decode(utf8_input, &output);

    /* Euro sign should be replaced with '?' (not available in ASCII) */
    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("?", output);

    free(output);
}

static void test_utf8_decode_mixed_ascii_and_utf8(void)
{
    char *output = NULL;

    /* "Hello" + space + accented e (0xC3 0xA9) + "!" */
    const char utf8_input[] = {'H', 'e', 'l', 'l', 'o', ' ', (char)0xC3, (char)0xA9, '!', 0};

    int result = utf8_decode(utf8_input, &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("Hello ?!", output);

    free(output);
}

static void test_utf8_decode_multiple_non_ascii(void)
{
    char *output = NULL;

    /* Two accented e characters: 0xC3 0xA9 0xC3 0xA9 */
    const char utf8_input[] = {(char)0xC3, (char)0xA9, (char)0xC3, (char)0xA9, 0};

    int result = utf8_decode(utf8_input, &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("??", output);

    free(output);
}

static void test_utf8_decode_single_char(void)
{
    char *output = NULL;
    int result = utf8_decode("X", &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("X", output);

    free(output);
}

static void test_utf8_decode_spaces_preserved(void)
{
    char *output = NULL;
    int result = utf8_decode("  a  b  ", &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("  a  b  ", output);

    free(output);
}

static void test_utf8_decode_newlines_preserved(void)
{
    char *output = NULL;
    int result = utf8_decode("line1\nline2\n", &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("line1\nline2\n", output);

    free(output);
}

static void test_utf8_decode_tabs_preserved(void)
{
    char *output = NULL;
    int result = utf8_decode("col1\tcol2", &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("col1\tcol2", output);

    free(output);
}

/* =================================================================
 * Tests for utf8_decode with various UTF-8 multi-byte sequences
 * ================================================================= */

static void test_utf8_decode_two_byte_sequence(void)
{
    char *output = NULL;

    /* Latin small letter n with tilde: U+00F1 -> 0xC3 0xB1 */
    const char utf8_input[] = {(char)0xC3, (char)0xB1, 0};

    int result = utf8_decode(utf8_input, &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("?", output);

    free(output);
}

static void test_utf8_decode_three_byte_sequence(void)
{
    char *output = NULL;

    /* Chinese character (example): 0xE4 0xB8 0xAD */
    const char utf8_input[] = {(char)0xE4, (char)0xB8, (char)0xAD, 0};

    int result = utf8_decode(utf8_input, &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("?", output);

    free(output);
}

static void test_utf8_decode_four_byte_sequence(void)
{
    char *output = NULL;

    /* Emoji (grinning face): U+1F600 -> 0xF0 0x9F 0x98 0x80 */
    const char utf8_input[] = {(char)0xF0, (char)0x9F, (char)0x98, (char)0x80, 0};

    int result = utf8_decode(utf8_input, &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    /* Emoji should be replaced with '?' */
    TEST_ASSERT_EQUAL_STRING("?", output);

    free(output);
}

/* =================================================================
 * Tests for utf8_decode with edge cases
 * ================================================================= */

static void test_utf8_decode_long_ascii_string(void)
{
    char *output = NULL;
    const char *long_string =
        "This is a longer ASCII string that should be converted exactly "
        "without any modifications because all characters are within the "
        "standard ASCII range of 0-127.";

    int result = utf8_decode(long_string, &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING(long_string, output);

    free(output);
}

static void test_utf8_decode_all_printable_ascii(void)
{
    char *output = NULL;
    /* All printable ASCII characters from space (32) to tilde (126) */
    char printable[96];
    for (int i = 0; i < 95; i++) {
        printable[i] = (char)(32 + i);
    }
    printable[95] = '\0';

    int result = utf8_decode(printable, &output);

    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING(printable, output);

    free(output);
}

static void test_utf8_decode_control_chars(void)
{
    char *output = NULL;
    /* Some control characters: bell, backspace, form feed */
    const char control[] = {0x07, 0x08, 0x0C, 0};

    int result = utf8_decode(control, &output);

    /* Control chars are valid ASCII, should pass through */
    TEST_ASSERT_TRUE(result >= 0);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL(3, strlen(output));

    free(output);
}

/* =================================================================
 * Tests for utf8_decode with potentially invalid UTF-8
 * ================================================================= */

static void test_utf8_decode_invalid_lead_byte(void)
{
    char *output = NULL;

    /* 0xFF is never valid in UTF-8 */
    const char invalid[] = {(char)0xFF, 0};

    int result = utf8_decode(invalid, &output);

    /* Should handle gracefully - either error or replacement */
    TEST_ASSERT_NOT_NULL(output);

    free(output);
}

static void test_utf8_decode_orphan_continuation(void)
{
    char *output = NULL;

    /* 0x80-0xBF are continuation bytes, invalid as lead byte */
    const char invalid[] = {(char)0x80, 0};

    int result = utf8_decode(invalid, &output);

    /* Should handle gracefully */
    TEST_ASSERT_NOT_NULL(output);

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

    /* Basic utf8_decode tests */
    RUN_TEST(test_utf8_decode_empty_string);
    RUN_TEST(test_utf8_decode_ascii_only);
    RUN_TEST(test_utf8_decode_with_numbers);
    RUN_TEST(test_utf8_decode_special_ascii_chars);
    RUN_TEST(test_utf8_decode_single_char);
    RUN_TEST(test_utf8_decode_spaces_preserved);
    RUN_TEST(test_utf8_decode_newlines_preserved);
    RUN_TEST(test_utf8_decode_tabs_preserved);

    /* Non-ASCII replacement tests */
    RUN_TEST(test_utf8_decode_non_ascii_replaced);
    RUN_TEST(test_utf8_decode_euro_sign);
    RUN_TEST(test_utf8_decode_mixed_ascii_and_utf8);
    RUN_TEST(test_utf8_decode_multiple_non_ascii);

    /* Multi-byte sequence tests */
    RUN_TEST(test_utf8_decode_two_byte_sequence);
    RUN_TEST(test_utf8_decode_three_byte_sequence);
    RUN_TEST(test_utf8_decode_four_byte_sequence);

    /* Edge case tests */
    RUN_TEST(test_utf8_decode_long_ascii_string);
    RUN_TEST(test_utf8_decode_all_printable_ascii);
    RUN_TEST(test_utf8_decode_control_chars);

    /* Invalid UTF-8 tests */
    RUN_TEST(test_utf8_decode_invalid_lead_byte);
    RUN_TEST(test_utf8_decode_orphan_continuation);

    return UNITY_END();
}
