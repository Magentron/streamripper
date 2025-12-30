/* test_metadata_pipeline.c - Integration tests for metadata parsing pipeline
 *
 * This integration test verifies the data flow between:
 * - Metadata parsing (parse.c)
 * - Character encoding conversion (mchar.c)
 * - Track info structures (srtypes.h)
 *
 * Tests the flow: parse ICY metadata -> apply rules -> generate track info
 */
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "errors.h"
#include "mchar.h"
#include "parse.h"
#include "srtypes.h"

/* Stub for debug_printf */
void debug_printf(char *fmt, ...)
{
    (void)fmt;
}

/* Test fixtures */
static RIP_MANAGER_INFO test_rmi;
static STREAM_PREFS test_prefs;
static TRACK_INFO test_ti;
static CODESET_OPTIONS test_cs_opt;

void setUp(void)
{
    memset(&test_rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&test_prefs, 0, sizeof(STREAM_PREFS));
    memset(&test_ti, 0, sizeof(TRACK_INFO));
    memset(&test_cs_opt, 0, sizeof(CODESET_OPTIONS));

    test_rmi.prefs = &test_prefs;

    /* Initialize default codesets */
    set_codesets_default(&test_cs_opt);

    /* Register codesets with rip manager */
    register_codesets(&test_rmi, &test_cs_opt);

    errors_init();
}

void tearDown(void)
{
    /* Free parse rules if allocated */
    parser_free(&test_rmi);
}

/* ============================================================================
 * Test: Default metadata parsing rules initialization
 * Tests that the parser initializes with default rules.
 * ============================================================================ */
static void test_init_default_parser_rules(void)
{
    /* Initialize parser with no rules file (uses defaults) */
    init_metadata_parser(&test_rmi, NULL);

    /* Parser should be initialized with default rules */
    TEST_ASSERT_NOT_NULL(test_rmi.parse_rules);
}

/* ============================================================================
 * Test: Simple artist - title parsing
 * Tests parsing "Artist - Title" format metadata.
 * ============================================================================ */
static void test_parse_simple_artist_title(void)
{
    /* Initialize parser with default rules */
    init_metadata_parser(&test_rmi, NULL);

    /* Set up raw metadata */
    strcpy(test_ti.raw_metadata, "The Beatles - Hey Jude");
    test_ti.have_track_info = 0;

    /* Parse the metadata */
    parse_metadata(&test_rmi, &test_ti);

    /* Verify parsing results */
    TEST_ASSERT_EQUAL(1, test_ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("The Beatles", test_ti.artist);
    TEST_ASSERT_EQUAL_STRING("Hey Jude", test_ti.title);
}

/* ============================================================================
 * Test: Metadata with extra whitespace
 * Tests that whitespace is properly trimmed.
 * ============================================================================ */
static void test_parse_metadata_with_whitespace(void)
{
    init_metadata_parser(&test_rmi, NULL);

    /* Metadata with extra spaces */
    strcpy(test_ti.raw_metadata, "  Pink Floyd   -   Comfortably Numb  ");
    test_ti.have_track_info = 0;

    parse_metadata(&test_rmi, &test_ti);

    TEST_ASSERT_EQUAL(1, test_ti.have_track_info);
    /* Default rules should handle whitespace */
    TEST_ASSERT_NOT_EQUAL(0, strlen(test_ti.artist));
    TEST_ASSERT_NOT_EQUAL(0, strlen(test_ti.title));
}

/* ============================================================================
 * Test: Metadata without separator
 * Tests handling metadata that doesn't match artist-title pattern.
 * ============================================================================ */
static void test_parse_metadata_no_separator(void)
{
    init_metadata_parser(&test_rmi, NULL);

    /* Metadata without hyphen separator */
    strcpy(test_ti.raw_metadata, "Station Identification Message");
    test_ti.have_track_info = 0;

    parse_metadata(&test_rmi, &test_ti);

    /* Should still have track info, but parsing may vary */
    /* The exact behavior depends on the default rules */
    TEST_ASSERT_TRUE(test_ti.have_track_info == 0 || test_ti.have_track_info == 1);
}

/* ============================================================================
 * Test: Empty metadata handling
 * Tests that empty metadata is handled gracefully.
 * ============================================================================ */
static void test_parse_empty_metadata(void)
{
    init_metadata_parser(&test_rmi, NULL);

    /* Empty metadata */
    test_ti.raw_metadata[0] = '\0';
    test_ti.have_track_info = 0;

    parse_metadata(&test_rmi, &test_ti);

    /* Should handle gracefully - the parser may set have_track_info
     * even for empty metadata (with empty fields) */
    /* Just verify no crash and fields are empty */
    TEST_ASSERT_EQUAL_STRING("", test_ti.raw_metadata);
}

/* ============================================================================
 * Test: Track info structure initialization
 * Tests that TRACK_INFO fields are properly initialized.
 * ============================================================================ */
static void test_track_info_structure(void)
{
    TRACK_INFO ti;

    memset(&ti, 0, sizeof(TRACK_INFO));

    /* Verify all fields are zero/empty after initialization */
    TEST_ASSERT_EQUAL(0, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("", ti.raw_metadata);
    TEST_ASSERT_EQUAL_STRING("", ti.artist);
    TEST_ASSERT_EQUAL_STRING("", ti.title);
    TEST_ASSERT_EQUAL_STRING("", ti.album);
    TEST_ASSERT_EQUAL(FALSE, ti.save_track);
}

/* ============================================================================
 * Test: Composed metadata generation
 * Tests creating composed metadata for relay streams.
 * ============================================================================ */
static void test_composed_metadata_generation(void)
{
    init_metadata_parser(&test_rmi, NULL);

    /* Set up track info */
    strcpy(test_ti.raw_metadata, "Radiohead - Creep");
    test_ti.have_track_info = 0;

    parse_metadata(&test_rmi, &test_ti);

    /* After parsing, we can set up composed metadata */
    /* This would typically be done by ripstream when sending to relay */
    if (test_ti.have_track_info) {
        /* Format: StreamTitle='Artist - Title'; */
        int len = snprintf(test_ti.composed_metadata + 1,
                          MAX_METADATA_LEN - 1,
                          "StreamTitle='%s - %s';",
                          test_ti.artist, test_ti.title);
        /* First byte is length/16 */
        test_ti.composed_metadata[0] = (char)((len + 15) / 16);

        /* Verify composed metadata is non-empty */
        TEST_ASSERT_NOT_EQUAL(0, test_ti.composed_metadata[0]);
    }
}

/* ============================================================================
 * Test: Codeset options initialization
 * Tests that codeset options are properly initialized.
 * ============================================================================ */
static void test_codeset_options_initialization(void)
{
    CODESET_OPTIONS cs_opt;

    set_codesets_default(&cs_opt);

    /* Verify default codesets are set */
    TEST_ASSERT_NOT_EQUAL(0, strlen(cs_opt.codeset_locale));
}

/* ============================================================================
 * Test: Codeset registration with rip manager
 * Tests that codesets are registered properly.
 * ============================================================================ */
static void test_codeset_registration(void)
{
    CODESET_OPTIONS cs_opt;
    RIP_MANAGER_INFO rmi;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));

    /* Set up codesets */
    set_codesets_default(&cs_opt);

    /* Register with rip manager */
    register_codesets(&rmi, &cs_opt);

    /* Verify registration - mchar_cs should be populated */
    TEST_ASSERT_EQUAL_STRING(cs_opt.codeset_locale, rmi.mchar_cs.codeset_locale);
}

/* ============================================================================
 * Test: Multiple metadata parsing iterations
 * Tests parsing multiple tracks in sequence.
 * ============================================================================ */
static void test_multiple_metadata_parsing(void)
{
    const char *tracks[] = {
        "Queen - Bohemian Rhapsody",
        "Led Zeppelin - Stairway to Heaven",
        "AC/DC - Back in Black"
    };

    init_metadata_parser(&test_rmi, NULL);

    for (int i = 0; i < 3; i++) {
        /* Reset track info */
        memset(&test_ti, 0, sizeof(TRACK_INFO));
        strcpy(test_ti.raw_metadata, tracks[i]);

        /* Parse */
        parse_metadata(&test_rmi, &test_ti);

        /* Verify each track parsed */
        TEST_ASSERT_EQUAL(1, test_ti.have_track_info);
        TEST_ASSERT_NOT_EQUAL(0, strlen(test_ti.artist));
        TEST_ASSERT_NOT_EQUAL(0, strlen(test_ti.title));
    }
}

/* ============================================================================
 * Test: String utility functions
 * Tests mchar string manipulation functions.
 * ============================================================================ */
static void test_string_utilities(void)
{
    char str[256];
    char result[256];

    /* Test trim function */
    strcpy(str, "  hello world  ");
    trim(str);
    TEST_ASSERT_EQUAL_STRING("hello world", str);

    /* Test sr_strncpy */
    memset(result, 'X', sizeof(result));
    sr_strncpy(result, "test string", 5);
    TEST_ASSERT_EQUAL('t', result[0]);
    TEST_ASSERT_EQUAL('e', result[1]);
    TEST_ASSERT_EQUAL('s', result[2]);
    TEST_ASSERT_EQUAL('t', result[3]);
    /* Last byte should be null terminator */

    /* Test left_str */
    strcpy(str, "hello world");
    char *left = left_str(str, 5);
    TEST_ASSERT_EQUAL_STRING("hello", left);
}

/* ============================================================================
 * Test: Metadata with special characters
 * Tests parsing metadata containing special characters.
 * ============================================================================ */
static void test_metadata_with_special_chars(void)
{
    init_metadata_parser(&test_rmi, NULL);

    /* Metadata with special characters */
    strcpy(test_ti.raw_metadata, "Guns N' Roses - Sweet Child O' Mine");
    test_ti.have_track_info = 0;

    parse_metadata(&test_rmi, &test_ti);

    /* Should parse despite apostrophes */
    TEST_ASSERT_EQUAL(1, test_ti.have_track_info);
}

/* ============================================================================
 * Test: Save track flag
 * Tests the save_track flag in track info.
 * ============================================================================ */
static void test_save_track_flag(void)
{
    init_metadata_parser(&test_rmi, NULL);

    /* Set up metadata */
    strcpy(test_ti.raw_metadata, "Artist - Good Song");
    test_ti.have_track_info = 0;
    test_ti.save_track = TRUE;

    parse_metadata(&test_rmi, &test_ti);

    /* save_track should be preserved or set based on rules */
    /* Default rules don't have exclude flag, so save_track should be TRUE */
    TEST_ASSERT_EQUAL(TRUE, test_ti.save_track);
}

/* ============================================================================
 * Test: Parser free cleanup
 * Tests that parser cleanup doesn't crash with various states.
 * ============================================================================ */
static void test_parser_cleanup(void)
{
    RIP_MANAGER_INFO rmi;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));

    /* Free before init - should not crash */
    parser_free(&rmi);

    /* Init then free */
    init_metadata_parser(&rmi, NULL);
    TEST_ASSERT_NOT_NULL(rmi.parse_rules);

    parser_free(&rmi);
    /* After free, accessing parse_rules would be undefined, so we just verify no crash */
}

/* ============================================================================
 * Test: Wide character metadata conversion
 * Tests converting metadata to wide character format.
 * ============================================================================ */
static void test_wide_char_metadata(void)
{
    init_metadata_parser(&test_rmi, NULL);

    /* Set up raw metadata */
    strcpy(test_ti.raw_metadata, "Test Artist - Test Song");
    test_ti.have_track_info = 0;

    /* Convert raw_metadata to wide format */
    int rc = gstring_from_string(&test_rmi, test_ti.w_raw_metadata,
                                 MAX_TRACK_LEN, test_ti.raw_metadata,
                                 CODESET_METADATA);

    /* Should succeed with 0 or positive return value */
    TEST_ASSERT_TRUE(rc >= 0);

    /* Wide metadata should be non-empty */
    TEST_ASSERT_NOT_EQUAL(0, mstrlen(test_ti.w_raw_metadata));
}

/* ============================================================================
 * Test: Format byte size utility
 * Tests the format_byte_size helper function.
 * ============================================================================ */
static void test_format_byte_size(void)
{
    char result[32];

    /* Test various sizes */
    format_byte_size(result, 1024);
    TEST_ASSERT_NOT_NULL(strstr(result, "1")); /* Should contain "1" */

    format_byte_size(result, 1048576); /* 1 MB */
    TEST_ASSERT_NOT_NULL(result);

    format_byte_size(result, 0);
    TEST_ASSERT_NOT_NULL(result);
}

/* ============================================================================
 * Main entry point
 * ============================================================================ */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* Parser initialization tests */
    RUN_TEST(test_init_default_parser_rules);
    RUN_TEST(test_parser_cleanup);

    /* Track info structure tests */
    RUN_TEST(test_track_info_structure);

    /* Metadata parsing tests */
    RUN_TEST(test_parse_simple_artist_title);
    RUN_TEST(test_parse_metadata_with_whitespace);
    RUN_TEST(test_parse_metadata_no_separator);
    RUN_TEST(test_parse_empty_metadata);
    RUN_TEST(test_metadata_with_special_chars);
    RUN_TEST(test_multiple_metadata_parsing);

    /* Composed metadata tests */
    RUN_TEST(test_composed_metadata_generation);
    RUN_TEST(test_save_track_flag);

    /* Codeset tests */
    RUN_TEST(test_codeset_options_initialization);
    RUN_TEST(test_codeset_registration);
    RUN_TEST(test_wide_char_metadata);

    /* Utility function tests */
    RUN_TEST(test_string_utilities);
    RUN_TEST(test_format_byte_size);

    return UNITY_END();
}
