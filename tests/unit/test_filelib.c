/* test_filelib.c - Unit tests for lib/filelib.c
 *
 * Tests for file library operations including path macros and constants.
 * Note: Most filelib functions require RIP_MANAGER_INFO which is complex
 * to set up, so we primarily test the macros and utility behavior.
 */
#include <string.h>
#include <unity.h>

#include "mchar.h"   /* Include mchar.h for m_() macro */
#include "filelib.h"
#include "errors.h"
#include "srtypes.h"

/* For testing internal functions, we can test the macros and constants */

void setUp(void)
{
    /* Nothing to set up */
}

void tearDown(void)
{
    /* Nothing to clean up */
}

/* Test: PATH_SLASH is defined correctly for Unix */
static void test_path_slash_unix(void)
{
    /* On Unix, PATH_SLASH should be '/' */
#if !WIN32
    char slash = '/';
    TEST_ASSERT_EQUAL(slash, PATH_SLASH);
    TEST_ASSERT_EQUAL_STRING("/", PATH_SLASH_STR);
#endif
}

/* Test: ISSLASH macro correctly identifies slash characters */
static void test_isslash_macro(void)
{
    /* Forward slash should always be a slash */
    TEST_ASSERT_TRUE(ISSLASH('/'));

    /* Regular characters should not be slashes */
    TEST_ASSERT_FALSE(ISSLASH('a'));
    TEST_ASSERT_FALSE(ISSLASH('0'));
    TEST_ASSERT_FALSE(ISSLASH(' '));
    TEST_ASSERT_FALSE(ISSLASH('\0'));
}

/* Test: IS_ABSOLUTE_PATH macro works correctly */
static void test_is_absolute_path_unix(void)
{
#if !WIN32
    /* Absolute paths start with / on Unix */
    TEST_ASSERT_TRUE(IS_ABSOLUTE_PATH("/home/user"));
    TEST_ASSERT_TRUE(IS_ABSOLUTE_PATH("/"));
    TEST_ASSERT_TRUE(IS_ABSOLUTE_PATH("/tmp/file.mp3"));

    /* Relative paths don't start with / */
    TEST_ASSERT_FALSE(IS_ABSOLUTE_PATH("home/user"));
    TEST_ASSERT_FALSE(IS_ABSOLUTE_PATH("./file.mp3"));
    TEST_ASSERT_FALSE(IS_ABSOLUTE_PATH("../file.mp3"));
    TEST_ASSERT_FALSE(IS_ABSOLUTE_PATH("file.mp3"));
#endif
}

/* Test: HAS_DEVICE macro for Unix (should always return 0) */
static void test_has_device_unix(void)
{
#if !WIN32
    /* On Unix, there are no device letters */
    TEST_ASSERT_FALSE(HAS_DEVICE("/home/user"));
    TEST_ASSERT_FALSE(HAS_DEVICE("C:/something"));
    TEST_ASSERT_FALSE(HAS_DEVICE("D:\\path"));
#endif
}

/* Test: SR_MAX_PATH and related constants are reasonable */
static void test_path_constants(void)
{
    /* SR_MAX_PATH should be a reasonable value */
    TEST_ASSERT_TRUE(SR_MAX_PATH >= 254);
    TEST_ASSERT_TRUE(SR_MAX_PATH <= 4096);

    /* SR_MIN_FILENAME should leave room for actual filenames */
    TEST_ASSERT_TRUE(SR_MIN_FILENAME > 0);
    TEST_ASSERT_TRUE(SR_MIN_FILENAME < SR_MAX_PATH);

    /* SR_MIN_COMPLETEDIR should be reasonable */
    TEST_ASSERT_TRUE(SR_MIN_COMPLETEDIR > 0);
    TEST_ASSERT_TRUE(SR_MIN_COMPLETEDIR < SR_MAX_PATH);

    /* SR_DATE_LEN should accommodate date strings */
    TEST_ASSERT_TRUE(SR_DATE_LEN >= 10);  /* At least YYYY-MM-DD */
}

/* Test: Maximum path calculations are consistent */
static void test_max_path_calculations(void)
{
    /* SR_MAX_INCOMPLETE should be less than SR_MAX_PATH */
    int max_incomplete = SR_MAX_PATH - SR_MIN_FILENAME;
    TEST_ASSERT_TRUE(max_incomplete > 0);
    TEST_ASSERT_TRUE(max_incomplete < SR_MAX_PATH);
}

/* Test: Content type constants are defined */
static void test_content_type_constants(void)
{
    /* Verify content type constants are unique and defined */
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_MP3, CONTENT_TYPE_OGG);
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_MP3, CONTENT_TYPE_AAC);
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_OGG, CONTENT_TYPE_AAC);
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_MP3, CONTENT_TYPE_NSV);
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_UNKNOWN, CONTENT_TYPE_MP3);
}

/* Test: OverwriteOpt enum values are distinct */
static void test_overwrite_opt_values(void)
{
    /* Verify overwrite options are distinct */
    TEST_ASSERT_NOT_EQUAL(OVERWRITE_UNKNOWN, OVERWRITE_ALWAYS);
    TEST_ASSERT_NOT_EQUAL(OVERWRITE_ALWAYS, OVERWRITE_NEVER);
    TEST_ASSERT_NOT_EQUAL(OVERWRITE_NEVER, OVERWRITE_LARGER);
    TEST_ASSERT_NOT_EQUAL(OVERWRITE_LARGER, OVERWRITE_VERSION);
}

/* Test: DATEBUF_LEN is sufficient for date strings */
static void test_datebuf_len(void)
{
    /* Date buffer should hold at least YYYY_MM_DD_HH_MM_SS format */
    /* Format: 2024_12_30_14_30_45 = 19 chars + null = 20 */
    TEST_ASSERT_TRUE(DATEBUF_LEN >= 20);
}

/* Test: FILELIB_INFO structure fields exist */
static void test_filelib_info_structure(void)
{
    FILELIB_INFO fli;

    /* Test that we can set and read fields */
    memset(&fli, 0, sizeof(FILELIB_INFO));

    fli.m_count = 5;
    TEST_ASSERT_EQUAL(5, fli.m_count);

    fli.m_do_show = TRUE;
    TEST_ASSERT_EQUAL(TRUE, fli.m_do_show);

    fli.m_keep_incomplete = FALSE;
    TEST_ASSERT_EQUAL(FALSE, fli.m_keep_incomplete);

    fli.m_max_filename_length = 100;
    TEST_ASSERT_EQUAL(100, fli.m_max_filename_length);

    fli.m_do_individual_tracks = TRUE;
    TEST_ASSERT_EQUAL(TRUE, fli.m_do_individual_tracks);

    fli.m_track_no = 3;
    TEST_ASSERT_EQUAL(3, fli.m_track_no);
}

/* Test: Path string arrays in FILELIB_INFO are sized correctly */
static void test_filelib_info_path_sizes(void)
{
    FILELIB_INFO fli;

    /* Verify path arrays are SR_MAX_PATH sized */
    TEST_ASSERT_EQUAL(SR_MAX_PATH, sizeof(fli.m_output_directory) / sizeof(mchar));
    TEST_ASSERT_EQUAL(SR_MAX_PATH, sizeof(fli.m_output_pattern) / sizeof(mchar));
    TEST_ASSERT_EQUAL(SR_MAX_PATH, sizeof(fli.m_incomplete_directory) / sizeof(mchar));
    TEST_ASSERT_EQUAL(SR_MAX_PATH, sizeof(fli.m_show_name) / sizeof(mchar));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_path_slash_unix);
    RUN_TEST(test_isslash_macro);
    RUN_TEST(test_is_absolute_path_unix);
    RUN_TEST(test_has_device_unix);
    RUN_TEST(test_path_constants);
    RUN_TEST(test_max_path_calculations);
    RUN_TEST(test_content_type_constants);
    RUN_TEST(test_overwrite_opt_values);
    RUN_TEST(test_datebuf_len);
    RUN_TEST(test_filelib_info_structure);
    RUN_TEST(test_filelib_info_path_sizes);

    return UNITY_END();
}
