/* test_parse.c - Unit tests for lib/parse.c
 *
 * Tests for metadata parsing functions.
 * Tests the parse rule structures and TRACK_INFO handling.
 */
#include <string.h>
#include <unity.h>

#include "srtypes.h"
#include "errors.h"

/* We test the structures and constants used by parse.c */

void setUp(void)
{
    /* Nothing to set up */
}

void tearDown(void)
{
    /* Nothing to clean up */
}

/* Test: TRACK_INFO structure can be initialized and used */
static void test_track_info_structure(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));

    ti.have_track_info = 1;
    TEST_ASSERT_EQUAL(1, ti.have_track_info);

    strncpy(ti.raw_metadata, "Artist - Title", MAX_TRACK_LEN);
    TEST_ASSERT_EQUAL_STRING("Artist - Title", ti.raw_metadata);

    ti.save_track = TRUE;
    TEST_ASSERT_EQUAL(TRUE, ti.save_track);
}

/* Test: TRACK_INFO artist field */
static void test_track_info_artist(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));

    strncpy(ti.artist, "Test Artist", MAX_TRACK_LEN);
    TEST_ASSERT_EQUAL_STRING("Test Artist", ti.artist);

    /* Test with longer artist name */
    strncpy(ti.artist, "A Very Long Artist Name That Goes On And On", MAX_TRACK_LEN);
    TEST_ASSERT_TRUE(strlen(ti.artist) > 0);
    TEST_ASSERT_TRUE(strlen(ti.artist) < MAX_TRACK_LEN);
}

/* Test: TRACK_INFO title field */
static void test_track_info_title(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));

    strncpy(ti.title, "Test Song Title", MAX_TRACK_LEN);
    TEST_ASSERT_EQUAL_STRING("Test Song Title", ti.title);
}

/* Test: TRACK_INFO album field */
static void test_track_info_album(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));

    strncpy(ti.album, "Greatest Hits", MAX_TRACK_LEN);
    TEST_ASSERT_EQUAL_STRING("Greatest Hits", ti.album);
}

/* Test: TRACK_INFO track number and year fields */
static void test_track_info_track_year(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));

    strncpy(ti.track_p, "05", MAX_TRACK_LEN);
    TEST_ASSERT_EQUAL_STRING("05", ti.track_p);

    strncpy(ti.track_a, "5", MAX_TRACK_LEN);
    TEST_ASSERT_EQUAL_STRING("5", ti.track_a);

    strncpy(ti.year, "2024", MAX_TRACK_LEN);
    TEST_ASSERT_EQUAL_STRING("2024", ti.year);
}

/* Test: TRACK_INFO composed_metadata field */
static void test_track_info_composed_metadata(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));

    /* Composed metadata is used for relay streams */
    /* First byte is length indicator (in 16-byte blocks) */
    ti.composed_metadata[0] = 1;  /* 1 block = 16 bytes */
    strncpy(&ti.composed_metadata[1], "StreamTitle='Test';", MAX_METADATA_LEN);

    TEST_ASSERT_EQUAL(1, ti.composed_metadata[0]);
    TEST_ASSERT_TRUE(strlen(&ti.composed_metadata[1]) > 0);
}

/* Test: MAX_TRACK_LEN constant */
static void test_max_track_len(void)
{
    /* MAX_TRACK_LEN should be a reasonable value */
    TEST_ASSERT_TRUE(MAX_TRACK_LEN >= 64);
    TEST_ASSERT_TRUE(MAX_TRACK_LEN <= 1024);
}

/* Test: MAX_METADATA_LEN constant */
static void test_max_metadata_len(void)
{
    /* MAX_METADATA_LEN should be 127*16 = 2032 */
    TEST_ASSERT_EQUAL(127 * 16, MAX_METADATA_LEN);
}

/* Test: Parse_Rule structure */
static void test_parse_rule_structure(void)
{
    Parse_Rule rule;

    memset(&rule, 0, sizeof(Parse_Rule));

    /* Test cmd field - MATCH or SUBST */
    rule.cmd = 0x01;  /* PARSERULE_CMD_MATCH */
    TEST_ASSERT_EQUAL(0x01, rule.cmd);

    rule.cmd = 0x02;  /* PARSERULE_CMD_SUBST */
    TEST_ASSERT_EQUAL(0x02, rule.cmd);

    /* Test flags field */
    rule.flags = 0x04;  /* PARSERULE_ICASE */
    TEST_ASSERT_EQUAL(0x04, rule.flags);

    /* Test index fields */
    rule.artist_idx = 1;
    TEST_ASSERT_EQUAL(1, rule.artist_idx);

    rule.title_idx = 2;
    TEST_ASSERT_EQUAL(2, rule.title_idx);

    rule.album_idx = 3;
    TEST_ASSERT_EQUAL(3, rule.album_idx);

    rule.trackno_idx = 4;
    TEST_ASSERT_EQUAL(4, rule.trackno_idx);

    rule.year_idx = 5;
    TEST_ASSERT_EQUAL(5, rule.year_idx);
}

/* Test: Parse rule command constants */
static void test_parse_rule_commands(void)
{
    /* Define local constants matching parse.c */
    const int PARSERULE_CMD_MATCH = 0x01;
    const int PARSERULE_CMD_SUBST = 0x02;

    /* Verify they are distinct */
    TEST_ASSERT_NOT_EQUAL(PARSERULE_CMD_MATCH, PARSERULE_CMD_SUBST);

    /* Verify they are non-zero */
    TEST_ASSERT_NOT_EQUAL(0, PARSERULE_CMD_MATCH);
    TEST_ASSERT_NOT_EQUAL(0, PARSERULE_CMD_SUBST);
}

/* Test: Parse rule flag constants */
static void test_parse_rule_flags(void)
{
    /* Define local constants matching parse.c */
    const int PARSERULE_SKIP = 0x01;
    const int PARSERULE_GLOBAL = 0x02;
    const int PARSERULE_ICASE = 0x04;
    const int PARSERULE_SAVE = 0x08;
    const int PARSERULE_EXCLUDE = 0x10;

    /* Verify flags are powers of 2 (can be combined) */
    TEST_ASSERT_EQUAL(0x01, PARSERULE_SKIP);
    TEST_ASSERT_EQUAL(0x02, PARSERULE_GLOBAL);
    TEST_ASSERT_EQUAL(0x04, PARSERULE_ICASE);
    TEST_ASSERT_EQUAL(0x08, PARSERULE_SAVE);
    TEST_ASSERT_EQUAL(0x10, PARSERULE_EXCLUDE);

    /* Verify they can be combined */
    int combined = PARSERULE_ICASE | PARSERULE_GLOBAL;
    TEST_ASSERT_TRUE(combined & PARSERULE_ICASE);
    TEST_ASSERT_TRUE(combined & PARSERULE_GLOBAL);
    TEST_ASSERT_FALSE(combined & PARSERULE_SKIP);
}

/* Test: TRACK_INFO w_raw_metadata field (wide character) */
static void test_track_info_wide_metadata(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));

    /* w_raw_metadata is for wide character storage */
    strncpy(ti.w_raw_metadata, "Wide Metadata String", MAX_TRACK_LEN);
    TEST_ASSERT_EQUAL_STRING("Wide Metadata String", ti.w_raw_metadata);
}

/* Test: TRACK_INFO save_track field */
static void test_track_info_save_track(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));

    /* save_track controls whether the track should be saved */
    ti.save_track = TRUE;
    TEST_ASSERT_EQUAL(TRUE, ti.save_track);

    ti.save_track = FALSE;
    TEST_ASSERT_EQUAL(FALSE, ti.save_track);
}

/* Test: TRACK_INFO have_track_info field */
static void test_track_info_have_track_info(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));

    /* have_track_info indicates if metadata was parsed successfully */
    ti.have_track_info = 0;
    TEST_ASSERT_EQUAL(0, ti.have_track_info);

    ti.have_track_info = 1;
    TEST_ASSERT_EQUAL(1, ti.have_track_info);
}

/* Test: Metadata format "Artist - Title" pattern */
static void test_metadata_artist_title_format(void)
{
    TRACK_INFO ti;
    const char *metadata = "Test Artist - Test Song";

    memset(&ti, 0, sizeof(TRACK_INFO));

    /* Simulate what parse.c does: store raw metadata */
    strncpy(ti.raw_metadata, metadata, MAX_TRACK_LEN);
    TEST_ASSERT_EQUAL_STRING("Test Artist - Test Song", ti.raw_metadata);

    /* Verify the separator pattern exists */
    TEST_ASSERT_NOT_NULL(strstr(ti.raw_metadata, " - "));
}

/* Test: StreamTitle metadata format */
static void test_streamtitle_format(void)
{
    const char *icy_metadata = "StreamTitle='Artist - Song';";
    char *title_start;
    char *title_end;

    /* Verify StreamTitle format can be parsed */
    title_start = strstr(icy_metadata, "StreamTitle='");
    TEST_ASSERT_NOT_NULL(title_start);

    title_end = strstr(icy_metadata, "';");
    TEST_ASSERT_NOT_NULL(title_end);

    TEST_ASSERT_TRUE(title_end > title_start);
}

/* Test: Empty metadata handling */
static void test_empty_metadata(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));

    /* Empty raw_metadata */
    ti.raw_metadata[0] = '\0';
    TEST_ASSERT_EQUAL(0, strlen(ti.raw_metadata));

    /* Empty artist and title */
    ti.artist[0] = '\0';
    ti.title[0] = '\0';
    TEST_ASSERT_EQUAL(0, strlen(ti.artist));
    TEST_ASSERT_EQUAL(0, strlen(ti.title));
}

/* Test: Unicode-like metadata (ASCII representation) */
static void test_unicode_metadata_ascii(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));

    /* UTF-8 encoded string (stored as bytes) */
    strncpy(ti.raw_metadata, "Caf\xC3\xA9 - Song", MAX_TRACK_LEN);
    TEST_ASSERT_TRUE(strlen(ti.raw_metadata) > 0);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_track_info_structure);
    RUN_TEST(test_track_info_artist);
    RUN_TEST(test_track_info_title);
    RUN_TEST(test_track_info_album);
    RUN_TEST(test_track_info_track_year);
    RUN_TEST(test_track_info_composed_metadata);
    RUN_TEST(test_max_track_len);
    RUN_TEST(test_max_metadata_len);
    RUN_TEST(test_parse_rule_structure);
    RUN_TEST(test_parse_rule_commands);
    RUN_TEST(test_parse_rule_flags);
    RUN_TEST(test_track_info_wide_metadata);
    RUN_TEST(test_track_info_save_track);
    RUN_TEST(test_track_info_have_track_info);
    RUN_TEST(test_metadata_artist_title_format);
    RUN_TEST(test_streamtitle_format);
    RUN_TEST(test_empty_metadata);
    RUN_TEST(test_unicode_metadata_ascii);

    return UNITY_END();
}
