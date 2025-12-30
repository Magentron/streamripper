/* test_e2e_metadata_changes.c
 *
 * E2E tests for metadata change detection and handling.
 *
 * These tests focus on:
 * - TRACK_INFO structure initialization and usage
 * - Metadata parsing configuration
 * - Track info handling end-to-end
 * - Metadata field validation
 *
 * Note: Full streaming metadata tests require the mock server.
 * These tests validate the metadata handling infrastructure.
 */

#include <string.h>
#include <unity.h>

#include "errors.h"
#include "http.h"
#include "rip_manager.h"
#include "srtypes.h"

#include "test_e2e_helpers.h"

/* Test fixtures */
static STREAM_PREFS test_prefs;
static TRACK_INFO test_track;
static char temp_dir[256];

/* ========================================================================== */
/* Setup and Teardown */
/* ========================================================================== */

void setUp(void)
{
    /* Create temp directory */
    if (!create_temp_directory(temp_dir, sizeof(temp_dir), "e2e_metadata")) {
        strcpy(temp_dir, "/tmp/e2e_metadata_test");
    }

    /* Initialize prefs */
    memset(&test_prefs, 0, sizeof(test_prefs));
    set_rip_manager_options_defaults(&test_prefs);

    /* Initialize track info */
    memset(&test_track, 0, sizeof(test_track));
}

void tearDown(void)
{
    remove_directory_recursive(temp_dir);
}

/* ========================================================================== */
/* Tests: TRACK_INFO structure */
/* ========================================================================== */

static void test_track_info_init_empty(void)
{
    TRACK_INFO ti;
    memset(&ti, 0, sizeof(ti));

    /* Verify all fields are zero/empty */
    TEST_ASSERT_EQUAL(0, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("", ti.raw_metadata);
    TEST_ASSERT_EQUAL_STRING("", ti.artist);
    TEST_ASSERT_EQUAL_STRING("", ti.title);
    TEST_ASSERT_EQUAL_STRING("", ti.album);
}

static void test_track_info_set_raw_metadata(void)
{
    TRACK_INFO ti;
    memset(&ti, 0, sizeof(ti));

    /* Set raw metadata */
    strncpy(ti.raw_metadata, "Test Artist - Test Song", sizeof(ti.raw_metadata) - 1);
    ti.have_track_info = 1;

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("Test Artist - Test Song", ti.raw_metadata);
}

static void test_track_info_set_artist_title(void)
{
    TRACK_INFO ti;
    memset(&ti, 0, sizeof(ti));

    /* Set artist and title separately */
    strncpy(ti.artist, "Test Artist", sizeof(ti.artist) - 1);
    strncpy(ti.title, "Test Song", sizeof(ti.title) - 1);
    ti.have_track_info = 1;

    TEST_ASSERT_EQUAL_STRING("Test Artist", ti.artist);
    TEST_ASSERT_EQUAL_STRING("Test Song", ti.title);
}

static void test_track_info_set_album(void)
{
    TRACK_INFO ti;
    memset(&ti, 0, sizeof(ti));

    strncpy(ti.album, "Test Album", sizeof(ti.album) - 1);

    TEST_ASSERT_EQUAL_STRING("Test Album", ti.album);
}

static void test_track_info_set_track_number(void)
{
    TRACK_INFO ti;
    memset(&ti, 0, sizeof(ti));

    /* Track number parsed from metadata */
    strncpy(ti.track_p, "5", sizeof(ti.track_p) - 1);
    /* Track number assigned to ID3 */
    strncpy(ti.track_a, "5", sizeof(ti.track_a) - 1);

    TEST_ASSERT_EQUAL_STRING("5", ti.track_p);
    TEST_ASSERT_EQUAL_STRING("5", ti.track_a);
}

static void test_track_info_set_year(void)
{
    TRACK_INFO ti;
    memset(&ti, 0, sizeof(ti));

    strncpy(ti.year, "2024", sizeof(ti.year) - 1);

    TEST_ASSERT_EQUAL_STRING("2024", ti.year);
}

static void test_track_info_save_track_flag(void)
{
    TRACK_INFO ti;
    memset(&ti, 0, sizeof(ti));

    /* Default should be FALSE */
    TEST_ASSERT_FALSE(ti.save_track);

    /* Set to TRUE */
    ti.save_track = TRUE;
    TEST_ASSERT_TRUE(ti.save_track);
}

static void test_track_info_composed_metadata(void)
{
    TRACK_INFO ti;
    memset(&ti, 0, sizeof(ti));

    /* Set composed metadata for relay stream */
    const char *composed = "StreamTitle='Test Artist - Test Song';";
    strncpy(ti.composed_metadata, composed, sizeof(ti.composed_metadata) - 1);

    TEST_ASSERT_EQUAL_STRING(composed, ti.composed_metadata);
}

/* ========================================================================== */
/* Tests: Metadata max lengths */
/* ========================================================================== */

static void test_track_info_max_length_raw_metadata(void)
{
    TRACK_INFO ti;
    memset(&ti, 0, sizeof(ti));

    /* Verify MAX_TRACK_LEN is defined and reasonable */
    TEST_ASSERT_TRUE(MAX_TRACK_LEN > 0);
    TEST_ASSERT_TRUE(MAX_TRACK_LEN < 10000);  /* Reasonable upper bound */

    /* Create a string at max length - 1 */
    char long_str[MAX_TRACK_LEN];
    memset(long_str, 'A', MAX_TRACK_LEN - 1);
    long_str[MAX_TRACK_LEN - 1] = '\0';

    strncpy(ti.raw_metadata, long_str, sizeof(ti.raw_metadata) - 1);
    ti.raw_metadata[sizeof(ti.raw_metadata) - 1] = '\0';

    /* Should not crash and should have copied data */
    TEST_ASSERT_EQUAL(MAX_TRACK_LEN - 1, strlen(ti.raw_metadata));
}

static void test_track_info_max_length_composed_metadata(void)
{
    TRACK_INFO ti;
    memset(&ti, 0, sizeof(ti));

    /* MAX_METADATA_LEN for composed metadata */
    TEST_ASSERT_TRUE(MAX_METADATA_LEN > 0);
    TEST_ASSERT_EQUAL(127 * 16, MAX_METADATA_LEN);  /* ICY spec: 127 * 16 = 2032 */
}

/* ========================================================================== */
/* Tests: Multiple track info structures */
/* ========================================================================== */

static void test_multiple_track_info_independence(void)
{
    TRACK_INFO ti1, ti2;
    memset(&ti1, 0, sizeof(ti1));
    memset(&ti2, 0, sizeof(ti2));

    /* Set different values */
    strncpy(ti1.artist, "Artist One", sizeof(ti1.artist) - 1);
    strncpy(ti2.artist, "Artist Two", sizeof(ti2.artist) - 1);

    /* Verify they are independent */
    TEST_ASSERT_EQUAL_STRING("Artist One", ti1.artist);
    TEST_ASSERT_EQUAL_STRING("Artist Two", ti2.artist);
    TEST_ASSERT_TRUE(strcmp(ti1.artist, ti2.artist) != 0);
}

static void test_track_info_copy(void)
{
    TRACK_INFO ti1, ti2;
    memset(&ti1, 0, sizeof(ti1));

    /* Set values in ti1 */
    strncpy(ti1.artist, "Test Artist", sizeof(ti1.artist) - 1);
    strncpy(ti1.title, "Test Song", sizeof(ti1.title) - 1);
    strncpy(ti1.album, "Test Album", sizeof(ti1.album) - 1);
    ti1.have_track_info = 1;

    /* Copy to ti2 */
    memcpy(&ti2, &ti1, sizeof(TRACK_INFO));

    /* Verify copy */
    TEST_ASSERT_EQUAL_STRING("Test Artist", ti2.artist);
    TEST_ASSERT_EQUAL_STRING("Test Song", ti2.title);
    TEST_ASSERT_EQUAL_STRING("Test Album", ti2.album);
    TEST_ASSERT_EQUAL(1, ti2.have_track_info);
}

/* ========================================================================== */
/* Tests: ICY metadata format */
/* ========================================================================== */

static void test_icy_metadata_format_simple(void)
{
    /* ICY metadata format: StreamTitle='Artist - Title'; */
    const char *icy_format = "StreamTitle='Test Artist - Test Song';";

    /* Verify format */
    TEST_ASSERT_NOT_NULL(strstr(icy_format, "StreamTitle="));
    TEST_ASSERT_NOT_NULL(strstr(icy_format, "';"));
}

static void test_icy_metadata_format_with_url(void)
{
    /* ICY metadata can include StreamUrl */
    const char *icy_format = "StreamTitle='Test Artist - Test Song';StreamUrl='http://example.com';";

    TEST_ASSERT_NOT_NULL(strstr(icy_format, "StreamTitle="));
    TEST_ASSERT_NOT_NULL(strstr(icy_format, "StreamUrl="));
}

static void test_icy_metadata_length_encoding(void)
{
    /* ICY metadata length is: actual_length / 16, rounded up
       Max length byte is 255, so max metadata is 255 * 16 = 4080 bytes
       But practical limit is 127 * 16 = 2032 (signed byte) */

    int max_length_byte = 255;
    int metadata_size = max_length_byte * 16;

    TEST_ASSERT_EQUAL(4080, metadata_size);

    /* Practical limit */
    int practical_limit = 127 * 16;
    TEST_ASSERT_EQUAL(2032, practical_limit);
    TEST_ASSERT_EQUAL(MAX_METADATA_LEN, practical_limit);
}

/* ========================================================================== */
/* Tests: Stream prefs metadata options */
/* ========================================================================== */

static void test_prefs_rules_file_setting(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default should be empty */
    TEST_ASSERT_EQUAL_STRING("", prefs.rules_file);

    /* Set rules file */
    strncpy(prefs.rules_file, "/path/to/rules.txt", sizeof(prefs.rules_file) - 1);
    TEST_ASSERT_EQUAL_STRING("/path/to/rules.txt", prefs.rules_file);
}

static void test_prefs_external_cmd_setting(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default should be empty */
    TEST_ASSERT_EQUAL_STRING("", prefs.ext_cmd);

    /* Set external command */
    strncpy(prefs.ext_cmd, "/usr/bin/metadata_script.sh", sizeof(prefs.ext_cmd) - 1);
    TEST_ASSERT_EQUAL_STRING("/usr/bin/metadata_script.sh", prefs.ext_cmd);
}

static void test_prefs_external_cmd_flag(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default should be OFF */
    TEST_ASSERT_FALSE(GET_EXTERNAL_CMD(prefs.flags));

    /* Turn on */
    OPT_FLAG_SET(prefs.flags, OPT_EXTERNAL_CMD, 1);
    TEST_ASSERT_TRUE(GET_EXTERNAL_CMD(prefs.flags));
}

/* ========================================================================== */
/* Tests: Track dropcount */
/* ========================================================================== */

static void test_prefs_dropcount_default(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Dropcount should be 1 by default (changed in version 1.63-beta-8) */
    TEST_ASSERT_EQUAL(1, prefs.dropcount);
}

static void test_prefs_dropcount_setting(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Set dropcount to skip first track */
    prefs.dropcount = 1;
    TEST_ASSERT_EQUAL(1, prefs.dropcount);

    /* Skip multiple tracks */
    prefs.dropcount = 3;
    TEST_ASSERT_EQUAL(3, prefs.dropcount);
}

/* ========================================================================== */
/* Tests: Count start for track numbering */
/* ========================================================================== */

static void test_prefs_count_start_default(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Count start should be 0 by default */
    TEST_ASSERT_EQUAL(0, prefs.count_start);
}

static void test_prefs_count_start_custom(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Start counting from 1 */
    prefs.count_start = 1;
    TEST_ASSERT_EQUAL(1, prefs.count_start);

    /* Start from higher number */
    prefs.count_start = 100;
    TEST_ASSERT_EQUAL(100, prefs.count_start);
}

/* ========================================================================== */
/* Tests: Output pattern */
/* ========================================================================== */

static void test_prefs_output_pattern_default(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default should be empty (use default pattern) */
    TEST_ASSERT_EQUAL_STRING("", prefs.output_pattern);
}

static void test_prefs_output_pattern_custom(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Set custom pattern with placeholders */
    strncpy(prefs.output_pattern, "%A - %T", sizeof(prefs.output_pattern) - 1);
    TEST_ASSERT_EQUAL_STRING("%A - %T", prefs.output_pattern);
}

static void test_prefs_showfile_pattern(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default should be empty */
    TEST_ASSERT_EQUAL_STRING("", prefs.showfile_pattern);

    /* Set showfile pattern */
    strncpy(prefs.showfile_pattern, "%S - %d", sizeof(prefs.showfile_pattern) - 1);
    TEST_ASSERT_EQUAL_STRING("%S - %d", prefs.showfile_pattern);
}

/* ========================================================================== */
/* Tests: Wide character metadata (mchar) */
/* ========================================================================== */

static void test_track_info_wide_raw_metadata(void)
{
    TRACK_INFO ti;
    memset(&ti, 0, sizeof(ti));

    /* The w_raw_metadata field is for wide character version */
    /* In modern streamripper, mchar is just gchar (UTF-8) */
    strncpy(ti.w_raw_metadata, "UTF-8 Metadata", sizeof(ti.w_raw_metadata) - 1);

    TEST_ASSERT_EQUAL_STRING("UTF-8 Metadata", ti.w_raw_metadata);
}

/* ========================================================================== */
/* Tests: NO_TRACK_STR constant */
/* ========================================================================== */

static void test_no_track_str_constant(void)
{
    /* Verify NO_TRACK_STR is defined */
    TEST_ASSERT_EQUAL_STRING("No track info...", NO_TRACK_STR);
}

/* ========================================================================== */
/* Main */
/* ========================================================================== */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    /* TRACK_INFO structure tests */
    RUN_TEST(test_track_info_init_empty);
    RUN_TEST(test_track_info_set_raw_metadata);
    RUN_TEST(test_track_info_set_artist_title);
    RUN_TEST(test_track_info_set_album);
    RUN_TEST(test_track_info_set_track_number);
    RUN_TEST(test_track_info_set_year);
    RUN_TEST(test_track_info_save_track_flag);
    RUN_TEST(test_track_info_composed_metadata);

    /* Metadata max lengths */
    RUN_TEST(test_track_info_max_length_raw_metadata);
    RUN_TEST(test_track_info_max_length_composed_metadata);

    /* Multiple track info */
    RUN_TEST(test_multiple_track_info_independence);
    RUN_TEST(test_track_info_copy);

    /* ICY metadata format */
    RUN_TEST(test_icy_metadata_format_simple);
    RUN_TEST(test_icy_metadata_format_with_url);
    RUN_TEST(test_icy_metadata_length_encoding);

    /* Stream prefs metadata options */
    RUN_TEST(test_prefs_rules_file_setting);
    RUN_TEST(test_prefs_external_cmd_setting);
    RUN_TEST(test_prefs_external_cmd_flag);

    /* Track dropcount */
    RUN_TEST(test_prefs_dropcount_default);
    RUN_TEST(test_prefs_dropcount_setting);

    /* Count start */
    RUN_TEST(test_prefs_count_start_default);
    RUN_TEST(test_prefs_count_start_custom);

    /* Output pattern */
    RUN_TEST(test_prefs_output_pattern_default);
    RUN_TEST(test_prefs_output_pattern_custom);
    RUN_TEST(test_prefs_showfile_pattern);

    /* Wide character metadata */
    RUN_TEST(test_track_info_wide_raw_metadata);

    /* NO_TRACK_STR */
    RUN_TEST(test_no_track_str_constant);

    return UNITY_END();
}
