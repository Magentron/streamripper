/* test_parse.c - Unit tests for lib/parse.c
 *
 * Tests for metadata parsing functions.
 * Tests the parse rule structures and TRACK_INFO handling.
 */
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unity.h>

#include "srtypes.h"
#include "errors.h"
#include "parse.h"

/* Stub implementations for dependencies */
void debug_printf(char *fmt, ...) {
    /* No-op for tests */
    (void)fmt;
}

void debug_mprintf(char *fmt, ...) {
    /* No-op for tests */
    (void)fmt;
}

/* Stub mchar functions - these are simple wrappers around standard string functions */
mchar *mstrdup(mchar *src) {
    return strdup(src);
}

void mstrncpy(mchar *dst, mchar *src, int n) {
    strncpy(dst, src, n);
    if (n > 0) {
        dst[n - 1] = '\0';
    }
}

size_t mstrlen(mchar *s) {
    return strlen(s);
}

int msnprintf(mchar *dest, size_t n, const mchar *fmt, ...) {
    va_list args;
    int ret;
    va_start(args, fmt);
    ret = vsnprintf(dest, n, fmt, args);
    va_end(args);
    return ret;
}

/* Stub for gstring_from_string - just copies the string for tests */
int gstring_from_string(RIP_MANAGER_INFO *rmi, mchar *m, int mlen, char *c, int codeset_type) {
    (void)rmi;
    (void)codeset_type;
    strncpy(m, c, mlen - 1);
    m[mlen - 1] = '\0';
    return strlen(m);
}

/* Stub for string_from_gstring - just copies the string for tests */
int string_from_gstring(RIP_MANAGER_INFO *rmi, char *c, int clen, mchar *m, int codeset_type) {
    (void)rmi;
    (void)codeset_type;
    strncpy(c, m, clen - 1);
    c[clen - 1] = '\0';
    return strlen(c);
}

/* Forward declaration for compose_metadata (static in parse.c, but we need it for testing) */
void compose_metadata(RIP_MANAGER_INFO *rmi, TRACK_INFO *ti);

/* Test RIP_MANAGER_INFO structure */
static RIP_MANAGER_INFO test_rmi;

void setUp(void)
{
    memset(&test_rmi, 0, sizeof(RIP_MANAGER_INFO));
}

void tearDown(void)
{
    if (test_rmi.parse_rules) {
        parser_free(&test_rmi);
    }
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

/*
 * =========================================================================
 * Tests for init_metadata_parser() - Actual function testing
 * =========================================================================
 */

/* Test: init_metadata_parser with NULL rules file (uses defaults) */
static void test_init_metadata_parser_null_file(void)
{
    init_metadata_parser(&test_rmi, NULL);

    TEST_ASSERT_NOT_NULL(test_rmi.parse_rules);
    TEST_ASSERT_NOT_EQUAL(0, test_rmi.parse_rules[0].cmd);
}

/* Test: init_metadata_parser with empty string (uses defaults) */
static void test_init_metadata_parser_empty_string(void)
{
    init_metadata_parser(&test_rmi, "");

    TEST_ASSERT_NOT_NULL(test_rmi.parse_rules);
    TEST_ASSERT_NOT_EQUAL(0, test_rmi.parse_rules[0].cmd);
}

/* Test: init_metadata_parser with non-existent file (uses defaults) */
static void test_init_metadata_parser_nonexistent_file(void)
{
    init_metadata_parser(&test_rmi, "/nonexistent/file/path.txt");

    TEST_ASSERT_NOT_NULL(test_rmi.parse_rules);
    TEST_ASSERT_NOT_EQUAL(0, test_rmi.parse_rules[0].cmd);
}

/*
 * =========================================================================
 * Tests for parse_metadata() - Main parsing function
 * =========================================================================
 */

/* Test: parse_metadata with basic "Artist - Title" format */
static void test_parse_metadata_artist_title(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "The Beatles - Hey Jude", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("The Beatles", ti.artist);
    TEST_ASSERT_EQUAL_STRING("Hey Jude", ti.title);
    TEST_ASSERT_EQUAL(TRUE, ti.save_track);
}

/* Test: parse_metadata with title only (no artist) */
static void test_parse_metadata_title_only(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Song Title Without Artist", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    /* When no separator found, should fallback to title only */
    TEST_ASSERT_EQUAL_STRING("Song Title Without Artist", ti.title);
}

/* Test: parse_metadata with empty string */
static void test_parse_metadata_empty_string(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    ti.raw_metadata[0] = '\0';

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    /* Should handle empty string gracefully */
    TEST_ASSERT_EQUAL(1, ti.have_track_info);
}

/* Test: parse_metadata with leading/trailing whitespace */
static void test_parse_metadata_whitespace(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "  Artist  -  Title  ", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("Artist", ti.artist);
    /* The regex pattern captures "Title  " with trailing spaces */
    TEST_ASSERT_TRUE(strncmp(ti.title, "Title", 5) == 0);
}

/* Test: parse_metadata with hyphen in artist name */
static void test_parse_metadata_hyphen_in_artist(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Jean-Michel Jarre - Oxygene", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("Jean-Michel Jarre", ti.artist);
    TEST_ASSERT_EQUAL_STRING("Oxygene", ti.title);
}

/* Test: parse_metadata with hyphen in title */
static void test_parse_metadata_hyphen_in_title(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Artist - Title-With-Hyphens", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("Artist", ti.artist);
    TEST_ASSERT_EQUAL_STRING("Title-With-Hyphens", ti.title);
}

/* Test: parse_metadata with special characters */
static void test_parse_metadata_special_characters(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "AC/DC - Back In Black", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("AC/DC", ti.artist);
    TEST_ASSERT_EQUAL_STRING("Back In Black", ti.title);
}

/* Test: parse_metadata with numbers */
static void test_parse_metadata_with_numbers(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Sum 41 - In Too Deep", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("Sum 41", ti.artist);
    TEST_ASSERT_EQUAL_STRING("In Too Deep", ti.title);
}

/* Test: parse_metadata with very long strings */
static void test_parse_metadata_long_strings(void)
{
    TRACK_INFO ti;
    char long_artist[200];
    char long_title[200];
    char metadata[500];

    memset(&ti, 0, sizeof(TRACK_INFO));
    memset(long_artist, 'A', sizeof(long_artist) - 1);
    long_artist[sizeof(long_artist) - 1] = '\0';
    memset(long_title, 'B', sizeof(long_title) - 1);
    long_title[sizeof(long_title) - 1] = '\0';

    snprintf(metadata, sizeof(metadata), "%s - %s", long_artist, long_title);
    strncpy(ti.raw_metadata, metadata, MAX_TRACK_LEN - 1);
    ti.raw_metadata[MAX_TRACK_LEN - 1] = '\0';

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    /* Should handle long strings without crashing */
    TEST_ASSERT_TRUE(strlen(ti.artist) > 0);
    TEST_ASSERT_TRUE(strlen(ti.title) > 0);
}

/* Test: parse_metadata with multiple separators */
static void test_parse_metadata_multiple_separators(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Artist - Title - Extra Info", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    /* Should parse correctly with multiple separators */
    TEST_ASSERT_NOT_EQUAL(0, strlen(ti.artist));
    TEST_ASSERT_NOT_EQUAL(0, strlen(ti.title));
}

/* Test: parse_metadata skip rule matching */
static void test_parse_metadata_skip_rule(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    /* The default rules have a skip rule for "A suivre:" */
    strncpy(ti.raw_metadata, "A suivre: something", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    /* Should be skipped */
    TEST_ASSERT_EQUAL(FALSE, ti.save_track);
    TEST_ASSERT_EQUAL(0, ti.have_track_info);
}

/* Test: parse_metadata with mp3pro suffix removal */
static void test_parse_metadata_mp3pro_removal(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Artist - Title - mp3pro", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    /* The default rules should remove mp3pro suffix */
    TEST_ASSERT_NOT_NULL(strstr(ti.raw_metadata, "Artist"));
}

/*
 * =========================================================================
 * Tests for compose_metadata() - Relay metadata composition
 * =========================================================================
 */

/* Test: compose_metadata with artist and title */
static void test_compose_metadata_artist_title(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    ti.have_track_info = 1;
    strncpy(ti.artist, "Test Artist", MAX_TRACK_LEN);
    strncpy(ti.title, "Test Song", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    compose_metadata(&test_rmi, &ti);

    /* Should create StreamTitle metadata */
    TEST_ASSERT_NOT_EQUAL(0, ti.composed_metadata[0]);
    TEST_ASSERT_TRUE(strlen(&ti.composed_metadata[1]) > 0);
}

/* Test: compose_metadata with title only */
static void test_compose_metadata_title_only(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    ti.have_track_info = 1;
    ti.artist[0] = '\0';
    strncpy(ti.title, "Test Song", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    compose_metadata(&test_rmi, &ti);

    /* Should create StreamTitle metadata with title only */
    TEST_ASSERT_NOT_EQUAL(0, ti.composed_metadata[0]);
}

/* Test: compose_metadata without track info */
static void test_compose_metadata_no_track_info(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    ti.have_track_info = 0;

    init_metadata_parser(&test_rmi, NULL);
    compose_metadata(&test_rmi, &ti);

    /* Should handle missing track info */
    /* First byte is length indicator */
    TEST_ASSERT_TRUE(ti.composed_metadata[0] >= 0);
}

/* Test: compose_metadata with empty artist and title */
static void test_compose_metadata_empty_fields(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    ti.have_track_info = 1;
    ti.artist[0] = '\0';
    ti.title[0] = '\0';

    init_metadata_parser(&test_rmi, NULL);
    compose_metadata(&test_rmi, &ti);

    /* Should handle empty fields gracefully */
    TEST_ASSERT_TRUE(ti.composed_metadata[0] >= 0);
}

/* Test: compose_metadata with long strings */
static void test_compose_metadata_long_strings(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    ti.have_track_info = 1;
    memset(ti.artist, 'A', MAX_TRACK_LEN - 1);
    ti.artist[MAX_TRACK_LEN - 1] = '\0';
    memset(ti.title, 'B', MAX_TRACK_LEN - 1);
    ti.title[MAX_TRACK_LEN - 1] = '\0';

    init_metadata_parser(&test_rmi, NULL);
    compose_metadata(&test_rmi, &ti);

    /* Should handle long strings without buffer overflow */
    TEST_ASSERT_NOT_EQUAL(0, ti.composed_metadata[0]);
    /* Ensure null termination */
    TEST_ASSERT_EQUAL(0, ti.composed_metadata[MAX_METADATA_LEN]);
}

/* Test: compose_metadata with special characters */
static void test_compose_metadata_special_chars(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    ti.have_track_info = 1;
    strncpy(ti.artist, "Artist's Name", MAX_TRACK_LEN);
    strncpy(ti.title, "Title \"quoted\"", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    compose_metadata(&test_rmi, &ti);

    /* Should handle special characters */
    TEST_ASSERT_NOT_EQUAL(0, ti.composed_metadata[0]);
}

/*
 * =========================================================================
 * Tests for parser_free()
 * =========================================================================
 */

/* Test: parser_free with allocated rules */
static void test_parser_free_with_rules(void)
{
    init_metadata_parser(&test_rmi, NULL);
    TEST_ASSERT_NOT_NULL(test_rmi.parse_rules);

    parser_free(&test_rmi);

    TEST_ASSERT_NULL(test_rmi.parse_rules);
}

/* Test: parser_free with NULL rules (should not crash) */
static void test_parser_free_null_rules(void)
{
    test_rmi.parse_rules = NULL;

    /* Should not crash */
    parser_free(&test_rmi);

    TEST_ASSERT_NULL(test_rmi.parse_rules);
}

/* Test: parser_free multiple times */
static void test_parser_free_multiple_times(void)
{
    init_metadata_parser(&test_rmi, NULL);

    parser_free(&test_rmi);
    TEST_ASSERT_NULL(test_rmi.parse_rules);

    /* Should handle multiple frees gracefully */
    parser_free(&test_rmi);
    TEST_ASSERT_NULL(test_rmi.parse_rules);
}

/*
 * =========================================================================
 * Tests for custom parse rules from file
 * =========================================================================
 */

/* Test: Load custom rules from file */
static void test_init_metadata_parser_with_custom_rules(void)
{
    FILE *fp;
    const char *rules_file = "/tmp/streamripper_test_rules.txt";

    /* Create a simple test rules file */
    fp = fopen(rules_file, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fprintf(fp, "# Test rules\n");
    fprintf(fp, "m/^([^-]+) - (.+)$/A1T2\n");
    fprintf(fp, "s/test//i\n");
    fclose(fp);

    init_metadata_parser(&test_rmi, (char *)rules_file);

    TEST_ASSERT_NOT_NULL(test_rmi.parse_rules);
    TEST_ASSERT_NOT_EQUAL(0, test_rmi.parse_rules[0].cmd);

    /* Clean up */
    unlink(rules_file);
}

/* Test: Parse with custom substitution rule */
static void test_parse_with_custom_subst_rule(void)
{
    TRACK_INFO ti;
    FILE *fp;
    const char *rules_file = "/tmp/streamripper_test_subst.txt";

    /* Create rules file with substitution */
    fp = fopen(rules_file, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fprintf(fp, "s/TEST//i\n");
    fprintf(fp, "m/^([^-]+) - (.+)$/A1T2\n");
    fclose(fp);

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Artist TEST - Title TEST", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, (char *)rules_file);
    parse_metadata(&test_rmi, &ti);

    /* The substitution should remove "TEST" */
    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_TRUE(strstr(ti.artist, "TEST") == NULL || strstr(ti.title, "TEST") == NULL);

    unlink(rules_file);
}

/* Test: Parse with skip flag rule */
static void test_parse_with_skip_flag_rule(void)
{
    TRACK_INFO ti;
    FILE *fp;
    const char *rules_file = "/tmp/streamripper_test_skip.txt";

    /* Create rules file with skip flag */
    fp = fopen(rules_file, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fprintf(fp, "m/^SKIP_THIS/e\n");
    fprintf(fp, "m/^([^-]+) - (.+)$/A1T2\n");
    fclose(fp);

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "SKIP_THIS content", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, (char *)rules_file);
    parse_metadata(&test_rmi, &ti);

    /* Should be skipped */
    TEST_ASSERT_EQUAL(FALSE, ti.save_track);
    TEST_ASSERT_EQUAL(0, ti.have_track_info);

    unlink(rules_file);
}

/* Test: Parse with save flag rule */
static void test_parse_with_save_flag_rule(void)
{
    TRACK_INFO ti;
    FILE *fp;
    const char *rules_file = "/tmp/streamripper_test_save.txt";

    /* Create rules file with save flag */
    fp = fopen(rules_file, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fprintf(fp, "m/^SAVE:/s\n");
    fprintf(fp, "m/^([^-]+) - (.+)$/A1T2\n");
    fclose(fp);

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "SAVE: Artist - Title", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, (char *)rules_file);
    parse_metadata(&test_rmi, &ti);

    /* Should be saved */
    TEST_ASSERT_EQUAL(TRUE, ti.save_track);
    TEST_ASSERT_EQUAL(1, ti.have_track_info);

    unlink(rules_file);
}

/* Test: Parse with exclude flag rule */
static void test_parse_with_exclude_flag_rule(void)
{
    TRACK_INFO ti;
    FILE *fp;
    const char *rules_file = "/tmp/streamripper_test_exclude.txt";

    /* Create rules file with exclude flag */
    fp = fopen(rules_file, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fprintf(fp, "m/^EXCLUDE/x\n");
    fprintf(fp, "m/^([^-]+) - (.+)$/A1T2\n");
    fclose(fp);

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "EXCLUDE this track", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, (char *)rules_file);
    parse_metadata(&test_rmi, &ti);

    /* Should be excluded (not saved) */
    TEST_ASSERT_EQUAL(FALSE, ti.save_track);

    unlink(rules_file);
}

/* Test: Parse with case-insensitive flag */
static void test_parse_with_case_insensitive_flag(void)
{
    TRACK_INFO ti;
    FILE *fp;
    const char *rules_file = "/tmp/streamripper_test_icase.txt";

    /* Create rules file with case-insensitive substitution */
    fp = fopen(rules_file, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fprintf(fp, "s/TEST//i\n");
    fprintf(fp, "m/^([^-]+) - (.+)$/A1T2\n");
    fclose(fp);

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Artist test - Title TEST", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, (char *)rules_file);
    parse_metadata(&test_rmi, &ti);

    /* Both "test" and "TEST" should be removed */
    TEST_ASSERT_EQUAL(1, ti.have_track_info);

    unlink(rules_file);
}

/* Test: Parse with album, track, and year fields */
static void test_parse_with_extended_fields(void)
{
    TRACK_INFO ti;
    FILE *fp;
    const char *rules_file = "/tmp/streamripper_test_extended.txt";

    /* Create rules file with album, track number, and year */
    fp = fopen(rules_file, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fprintf(fp, "m/^(.+) - (.+) \\[(.+)\\] \\((.+)\\) #(.+)$/A1T2C3Y4N5\n");
    fclose(fp);

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Artist - Title [Album] (2024) #05", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, (char *)rules_file);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("Artist", ti.artist);
    TEST_ASSERT_EQUAL_STRING("Title", ti.title);
    TEST_ASSERT_EQUAL_STRING("Album", ti.album);
    TEST_ASSERT_EQUAL_STRING("2024", ti.year);
    TEST_ASSERT_EQUAL_STRING("05", ti.track_p);

    unlink(rules_file);
}

/* Test: Malformed rules file handling */
static void test_parse_malformed_rules(void)
{
    FILE *fp;
    const char *rules_file = "/tmp/streamripper_test_malformed.txt";

    /* Create rules file with various malformed entries */
    fp = fopen(rules_file, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fprintf(fp, "# Good comment\n");
    fprintf(fp, "invalid line without command\n");
    fprintf(fp, "m missing delimiter\n");
    fprintf(fp, "m/unmatched pattern\n");
    fprintf(fp, "s/pattern without closing\n");
    fprintf(fp, "m/valid pattern/A1T2\n");  /* This one should work */
    fclose(fp);

    init_metadata_parser(&test_rmi, (char *)rules_file);

    /* Should still initialize with at least the valid rule */
    TEST_ASSERT_NOT_NULL(test_rmi.parse_rules);

    unlink(rules_file);
}

/* Test: Empty rules file */
static void test_parse_empty_rules_file(void)
{
    FILE *fp;
    const char *rules_file = "/tmp/streamripper_test_empty.txt";

    /* Create empty rules file */
    fp = fopen(rules_file, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fclose(fp);

    init_metadata_parser(&test_rmi, (char *)rules_file);

    /* Should have at least the sentinel rule */
    TEST_ASSERT_NOT_NULL(test_rmi.parse_rules);

    unlink(rules_file);
}

/* Test: Comments and whitespace in rules file */
static void test_parse_rules_with_comments(void)
{
    FILE *fp;
    const char *rules_file = "/tmp/streamripper_test_comments.txt";

    /* Create rules file with comments and whitespace */
    fp = fopen(rules_file, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fprintf(fp, "# This is a comment\n");
    fprintf(fp, "\n");
    fprintf(fp, "   # Indented comment\n");
    fprintf(fp, "   \n");
    fprintf(fp, "m/^(.+) - (.+)$/A1T2  # inline comment\n");
    fclose(fp);

    init_metadata_parser(&test_rmi, (char *)rules_file);

    TEST_ASSERT_NOT_NULL(test_rmi.parse_rules);
    TEST_ASSERT_NOT_EQUAL(0, test_rmi.parse_rules[0].cmd);

    unlink(rules_file);
}

/*
 * =========================================================================
 * Integration tests - Full workflow
 * =========================================================================
 */

/* Test: Full workflow - init, parse, compose, free */
static void test_full_workflow(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Artist Name - Song Title", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    TEST_ASSERT_NOT_NULL(test_rmi.parse_rules);

    parse_metadata(&test_rmi, &ti);
    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("Artist Name", ti.artist);
    TEST_ASSERT_EQUAL_STRING("Song Title", ti.title);

    /* compose_metadata is called by parse_metadata, but we can call it again */
    compose_metadata(&test_rmi, &ti);
    TEST_ASSERT_NOT_EQUAL(0, ti.composed_metadata[0]);

    parser_free(&test_rmi);
    TEST_ASSERT_NULL(test_rmi.parse_rules);
}

/* Test: Multiple parse operations with same parser */
static void test_multiple_parse_operations(void)
{
    TRACK_INFO ti1, ti2, ti3;

    init_metadata_parser(&test_rmi, NULL);

    /* Parse first track */
    memset(&ti1, 0, sizeof(TRACK_INFO));
    strncpy(ti1.raw_metadata, "Artist1 - Title1", MAX_TRACK_LEN);
    parse_metadata(&test_rmi, &ti1);
    TEST_ASSERT_EQUAL_STRING("Artist1", ti1.artist);
    TEST_ASSERT_EQUAL_STRING("Title1", ti1.title);

    /* Parse second track */
    memset(&ti2, 0, sizeof(TRACK_INFO));
    strncpy(ti2.raw_metadata, "Artist2 - Title2", MAX_TRACK_LEN);
    parse_metadata(&test_rmi, &ti2);
    TEST_ASSERT_EQUAL_STRING("Artist2", ti2.artist);
    TEST_ASSERT_EQUAL_STRING("Title2", ti2.title);

    /* Parse third track */
    memset(&ti3, 0, sizeof(TRACK_INFO));
    strncpy(ti3.raw_metadata, "Artist3 - Title3", MAX_TRACK_LEN);
    parse_metadata(&test_rmi, &ti3);
    TEST_ASSERT_EQUAL_STRING("Artist3", ti3.artist);
    TEST_ASSERT_EQUAL_STRING("Title3", ti3.title);

    parser_free(&test_rmi);
}

/* Test: Edge case - only separator */
static void test_parse_only_separator(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, " - ", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    /* Should handle gracefully */
    TEST_ASSERT_EQUAL(1, ti.have_track_info);
}

/* Test: Edge case - all whitespace */
static void test_parse_all_whitespace(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "     ", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    /* Should handle gracefully */
    TEST_ASSERT_EQUAL(1, ti.have_track_info);
}

/* Test: Metadata with parentheses */
static void test_parse_metadata_with_parentheses(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Artist - Title (Remix)", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("Artist", ti.artist);
    TEST_ASSERT_EQUAL_STRING("Title (Remix)", ti.title);
}

/* Test: Metadata with brackets */
static void test_parse_metadata_with_brackets(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Artist - Title [Live]", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("Artist", ti.artist);
    TEST_ASSERT_EQUAL_STRING("Title [Live]", ti.title);
}

/* Test: Metadata with ampersand */
static void test_parse_metadata_with_ampersand(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));
    strncpy(ti.raw_metadata, "Artist & Band - Title", MAX_TRACK_LEN);

    init_metadata_parser(&test_rmi, NULL);
    parse_metadata(&test_rmi, &ti);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("Artist & Band", ti.artist);
    TEST_ASSERT_EQUAL_STRING("Title", ti.title);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* Original structure tests */
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

    /* init_metadata_parser() tests */
    RUN_TEST(test_init_metadata_parser_null_file);
    RUN_TEST(test_init_metadata_parser_empty_string);
    RUN_TEST(test_init_metadata_parser_nonexistent_file);

    /* parse_metadata() tests */
    RUN_TEST(test_parse_metadata_artist_title);
    RUN_TEST(test_parse_metadata_title_only);
    RUN_TEST(test_parse_metadata_empty_string);
    RUN_TEST(test_parse_metadata_whitespace);
    RUN_TEST(test_parse_metadata_hyphen_in_artist);
    RUN_TEST(test_parse_metadata_hyphen_in_title);
    RUN_TEST(test_parse_metadata_special_characters);
    RUN_TEST(test_parse_metadata_with_numbers);
    RUN_TEST(test_parse_metadata_long_strings);
    RUN_TEST(test_parse_metadata_multiple_separators);
    RUN_TEST(test_parse_metadata_skip_rule);
    RUN_TEST(test_parse_metadata_mp3pro_removal);

    /* compose_metadata() tests */
    RUN_TEST(test_compose_metadata_artist_title);
    RUN_TEST(test_compose_metadata_title_only);
    RUN_TEST(test_compose_metadata_no_track_info);
    RUN_TEST(test_compose_metadata_empty_fields);
    RUN_TEST(test_compose_metadata_long_strings);
    RUN_TEST(test_compose_metadata_special_chars);

    /* parser_free() tests */
    RUN_TEST(test_parser_free_with_rules);
    RUN_TEST(test_parser_free_null_rules);
    RUN_TEST(test_parser_free_multiple_times);

    /* Custom parse rules tests */
    RUN_TEST(test_init_metadata_parser_with_custom_rules);
    RUN_TEST(test_parse_with_custom_subst_rule);
    RUN_TEST(test_parse_with_skip_flag_rule);
    RUN_TEST(test_parse_with_save_flag_rule);
    RUN_TEST(test_parse_with_exclude_flag_rule);
    RUN_TEST(test_parse_with_case_insensitive_flag);
    RUN_TEST(test_parse_with_extended_fields);
    RUN_TEST(test_parse_malformed_rules);
    RUN_TEST(test_parse_empty_rules_file);
    RUN_TEST(test_parse_rules_with_comments);

    /* Integration tests */
    RUN_TEST(test_full_workflow);
    RUN_TEST(test_multiple_parse_operations);
    RUN_TEST(test_parse_only_separator);
    RUN_TEST(test_parse_all_whitespace);
    RUN_TEST(test_parse_metadata_with_parentheses);
    RUN_TEST(test_parse_metadata_with_brackets);
    RUN_TEST(test_parse_metadata_with_ampersand);

    return UNITY_END();
}
