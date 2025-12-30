/* test_prefs.c - Unit tests for lib/prefs.c
 *
 * Tests for preference handling functions.
 * Note: Most prefs functions interact with a GKeyFile, which is complex
 * to mock. We test the helper functions and constants.
 */
#include <string.h>
#include <unity.h>

#include "srtypes.h"
#include "rip_manager.h"

/* Include for prefs_is_https which is a simple testable function */
/* We need to test the prefs structures and constants */

void setUp(void)
{
    /* Nothing to set up */
}

void tearDown(void)
{
    /* Nothing to clean up */
}

/* Test: DEFAULT_SKINFILE is defined */
static void test_default_skinfile_defined(void)
{
    /* DEFAULT_SKINFILE should be a non-empty string */
    TEST_ASSERT_TRUE(strlen(DEFAULT_SKINFILE) > 0);
}

/* Test: STREAM_PREFS structure fields exist and are accessible */
static void test_stream_prefs_structure(void)
{
    STREAM_PREFS prefs;

    memset(&prefs, 0, sizeof(STREAM_PREFS));

    /* Test string fields */
    strncpy(prefs.url, "http://example.com/stream", MAX_URL_LEN);
    TEST_ASSERT_EQUAL_STRING("http://example.com/stream", prefs.url);

    strncpy(prefs.proxyurl, "http://proxy:8080", MAX_URL_LEN);
    TEST_ASSERT_EQUAL_STRING("http://proxy:8080", prefs.proxyurl);

    strncpy(prefs.output_directory, "/tmp/output", SR_MAX_PATH);
    TEST_ASSERT_EQUAL_STRING("/tmp/output", prefs.output_directory);

    strncpy(prefs.output_pattern, "%S/%A - %T", SR_MAX_PATH);
    TEST_ASSERT_EQUAL_STRING("%S/%A - %T", prefs.output_pattern);

    strncpy(prefs.useragent, "TestAgent/1.0", MAX_USERAGENT_STR);
    TEST_ASSERT_EQUAL_STRING("TestAgent/1.0", prefs.useragent);
}

/* Test: STREAM_PREFS numeric fields */
static void test_stream_prefs_numeric_fields(void)
{
    STREAM_PREFS prefs;

    memset(&prefs, 0, sizeof(STREAM_PREFS));

    prefs.relay_port = 8000;
    TEST_ASSERT_EQUAL(8000, prefs.relay_port);

    prefs.max_port = 18000;
    TEST_ASSERT_EQUAL(18000, prefs.max_port);

    prefs.max_connections = 5;
    TEST_ASSERT_EQUAL(5, prefs.max_connections);

    prefs.maxMB_rip_size = 100;
    TEST_ASSERT_EQUAL(100, prefs.maxMB_rip_size);

    prefs.timeout = 30;
    TEST_ASSERT_EQUAL(30, prefs.timeout);

    prefs.dropcount = 2;
    TEST_ASSERT_EQUAL(2, prefs.dropcount);

    prefs.count_start = 1;
    TEST_ASSERT_EQUAL(1, prefs.count_start);
}

/* Test: STREAM_PREFS flags field and OPT_FLAG macros */
static void test_stream_prefs_flags(void)
{
    STREAM_PREFS prefs;

    memset(&prefs, 0, sizeof(STREAM_PREFS));

    /* Test setting and clearing flags */
    prefs.flags = 0;

    /* Set auto reconnect flag */
    OPT_FLAG_SET(prefs.flags, OPT_AUTO_RECONNECT, 1);
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(prefs.flags, OPT_AUTO_RECONNECT));

    /* Set separate dirs flag */
    OPT_FLAG_SET(prefs.flags, OPT_SEPARATE_DIRS, 1);
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(prefs.flags, OPT_SEPARATE_DIRS));

    /* Clear auto reconnect flag */
    OPT_FLAG_SET(prefs.flags, OPT_AUTO_RECONNECT, 0);
    TEST_ASSERT_FALSE(OPT_FLAG_ISSET(prefs.flags, OPT_AUTO_RECONNECT));

    /* Separate dirs should still be set */
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(prefs.flags, OPT_SEPARATE_DIRS));
}

/* Test: STREAM_PREFS overwrite option */
static void test_stream_prefs_overwrite(void)
{
    STREAM_PREFS prefs;

    memset(&prefs, 0, sizeof(STREAM_PREFS));

    prefs.overwrite = OVERWRITE_ALWAYS;
    TEST_ASSERT_EQUAL(OVERWRITE_ALWAYS, prefs.overwrite);

    prefs.overwrite = OVERWRITE_NEVER;
    TEST_ASSERT_EQUAL(OVERWRITE_NEVER, prefs.overwrite);

    prefs.overwrite = OVERWRITE_LARGER;
    TEST_ASSERT_EQUAL(OVERWRITE_LARGER, prefs.overwrite);

    prefs.overwrite = OVERWRITE_VERSION;
    TEST_ASSERT_EQUAL(OVERWRITE_VERSION, prefs.overwrite);
}

/* Test: SPLITPOINT_OPTIONS structure */
static void test_splitpoint_options_structure(void)
{
    SPLITPOINT_OPTIONS sp_opt;

    memset(&sp_opt, 0, sizeof(SPLITPOINT_OPTIONS));

    sp_opt.xs = 1;
    TEST_ASSERT_EQUAL(1, sp_opt.xs);

    sp_opt.xs_min_volume = 100;
    TEST_ASSERT_EQUAL(100, sp_opt.xs_min_volume);

    sp_opt.xs_silence_length = 1000;
    TEST_ASSERT_EQUAL(1000, sp_opt.xs_silence_length);

    sp_opt.xs_search_window_1 = 6000;
    TEST_ASSERT_EQUAL(6000, sp_opt.xs_search_window_1);

    sp_opt.xs_search_window_2 = 6000;
    TEST_ASSERT_EQUAL(6000, sp_opt.xs_search_window_2);

    sp_opt.xs_offset = 500;
    TEST_ASSERT_EQUAL(500, sp_opt.xs_offset);

    sp_opt.xs_padding_1 = 300;
    TEST_ASSERT_EQUAL(300, sp_opt.xs_padding_1);

    sp_opt.xs_padding_2 = 300;
    TEST_ASSERT_EQUAL(300, sp_opt.xs_padding_2);
}

/* Test: CODESET_OPTIONS structure */
static void test_codeset_options_structure(void)
{
    CODESET_OPTIONS cs_opt;

    memset(&cs_opt, 0, sizeof(CODESET_OPTIONS));

    strncpy(cs_opt.codeset_locale, "UTF-8", MAX_CODESET_STRING);
    TEST_ASSERT_EQUAL_STRING("UTF-8", cs_opt.codeset_locale);

    strncpy(cs_opt.codeset_filesys, "UTF-8", MAX_CODESET_STRING);
    TEST_ASSERT_EQUAL_STRING("UTF-8", cs_opt.codeset_filesys);

    strncpy(cs_opt.codeset_id3, "UTF-16", MAX_CODESET_STRING);
    TEST_ASSERT_EQUAL_STRING("UTF-16", cs_opt.codeset_id3);

    strncpy(cs_opt.codeset_metadata, "ISO-8859-1", MAX_CODESET_STRING);
    TEST_ASSERT_EQUAL_STRING("ISO-8859-1", cs_opt.codeset_metadata);

    strncpy(cs_opt.codeset_relay, "ISO-8859-1", MAX_CODESET_STRING);
    TEST_ASSERT_EQUAL_STRING("ISO-8859-1", cs_opt.codeset_relay);
}

/* Test: GLOBAL_PREFS structure */
static void test_global_prefs_structure(void)
{
    GLOBAL_PREFS global_prefs;

    memset(&global_prefs, 0, sizeof(GLOBAL_PREFS));

    strncpy(global_prefs.version, "1.67.1", MAX_VERSION_LEN);
    TEST_ASSERT_EQUAL_STRING("1.67.1", global_prefs.version);

    strncpy(global_prefs.url, "http://stream.example.com", MAX_URL_LEN);
    TEST_ASSERT_EQUAL_STRING("http://stream.example.com", global_prefs.url);

    /* Verify stream_prefs is embedded */
    global_prefs.stream_prefs.relay_port = 9000;
    TEST_ASSERT_EQUAL(9000, global_prefs.stream_prefs.relay_port);
}

/* Test: WSTREAMRIPPER_PREFS structure */
static void test_wstreamripper_prefs_structure(void)
{
    WSTREAMRIPPER_PREFS wsr_prefs;

    memset(&wsr_prefs, 0, sizeof(WSTREAMRIPPER_PREFS));

    wsr_prefs.m_add_finished_tracks_to_playlist = 1;
    TEST_ASSERT_EQUAL(1, wsr_prefs.m_add_finished_tracks_to_playlist);

    wsr_prefs.m_start_minimized = 0;
    TEST_ASSERT_EQUAL(0, wsr_prefs.m_start_minimized);

    wsr_prefs.oldpos_x = 100;
    TEST_ASSERT_EQUAL(100, wsr_prefs.oldpos_x);

    wsr_prefs.oldpos_y = 200;
    TEST_ASSERT_EQUAL(200, wsr_prefs.oldpos_y);

    wsr_prefs.m_enabled = 1;
    TEST_ASSERT_EQUAL(1, wsr_prefs.m_enabled);

    strncpy(wsr_prefs.localhost, "127.0.0.1", MAX_HOST_LEN);
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", wsr_prefs.localhost);
}

/* Test: RIPLIST_LEN constant */
static void test_riplist_len_constant(void)
{
    /* RIPLIST_LEN should be a reasonable value */
    TEST_ASSERT_TRUE(RIPLIST_LEN >= 5);
    TEST_ASSERT_TRUE(RIPLIST_LEN <= 100);
}

/* Test: WSTREAMRIPPER_PREFS riplist array */
static void test_wstreamripper_prefs_riplist(void)
{
    WSTREAMRIPPER_PREFS wsr_prefs;
    int i;

    memset(&wsr_prefs, 0, sizeof(WSTREAMRIPPER_PREFS));

    /* Test setting values in riplist */
    for (i = 0; i < RIPLIST_LEN && i < 3; i++) {
        char url[64];
        snprintf(url, sizeof(url), "http://stream%d.example.com", i);
        strncpy(wsr_prefs.riplist[i], url, MAX_URL_LEN);
    }

    TEST_ASSERT_EQUAL_STRING("http://stream0.example.com", wsr_prefs.riplist[0]);
    TEST_ASSERT_EQUAL_STRING("http://stream1.example.com", wsr_prefs.riplist[1]);
    TEST_ASSERT_EQUAL_STRING("http://stream2.example.com", wsr_prefs.riplist[2]);
}

/* Test: MAX constants are reasonable */
static void test_max_constants(void)
{
    TEST_ASSERT_TRUE(MAX_URL_LEN >= 1024);
    TEST_ASSERT_TRUE(MAX_URL_LEN <= 65536);

    TEST_ASSERT_TRUE(MAX_VERSION_LEN >= 16);
    TEST_ASSERT_TRUE(MAX_VERSION_LEN <= 512);

    TEST_ASSERT_TRUE(MAX_USERAGENT_STR >= 128);
    TEST_ASSERT_TRUE(MAX_USERAGENT_STR <= 2048);

    TEST_ASSERT_TRUE(MAX_CODESET_STRING >= 16);
    TEST_ASSERT_TRUE(MAX_CODESET_STRING <= 256);
}

/* Test: OPT_FLAG constants are distinct powers of 2 */
static void test_opt_flag_constants(void)
{
    /* Each OPT_FLAG should be a distinct power of 2 */
    /* This ensures they can be OR'd together without conflict */

    /* Verify some key flags are distinct */
    TEST_ASSERT_NOT_EQUAL(OPT_AUTO_RECONNECT, OPT_MAKE_RELAY);
    TEST_ASSERT_NOT_EQUAL(OPT_MAKE_RELAY, OPT_ADD_ID3V1);
    TEST_ASSERT_NOT_EQUAL(OPT_ADD_ID3V1, OPT_ADD_ID3V2);
    TEST_ASSERT_NOT_EQUAL(OPT_INDIVIDUAL_TRACKS, OPT_SINGLE_FILE_OUTPUT);
    TEST_ASSERT_NOT_EQUAL(OPT_SEPARATE_DIRS, OPT_DATE_STAMP);
}

/* Test: STREAM_PREFS http10 and wav_output fields */
static void test_stream_prefs_http_wav_fields(void)
{
    STREAM_PREFS prefs;

    memset(&prefs, 0, sizeof(STREAM_PREFS));

    prefs.http10 = 1;
    TEST_ASSERT_EQUAL(1, prefs.http10);

    prefs.wav_output = 1;
    TEST_ASSERT_EQUAL(1, prefs.wav_output);

    prefs.http10 = 0;
    prefs.wav_output = 0;
    TEST_ASSERT_EQUAL(0, prefs.http10);
    TEST_ASSERT_EQUAL(0, prefs.wav_output);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_default_skinfile_defined);
    RUN_TEST(test_stream_prefs_structure);
    RUN_TEST(test_stream_prefs_numeric_fields);
    RUN_TEST(test_stream_prefs_flags);
    RUN_TEST(test_stream_prefs_overwrite);
    RUN_TEST(test_splitpoint_options_structure);
    RUN_TEST(test_codeset_options_structure);
    RUN_TEST(test_global_prefs_structure);
    RUN_TEST(test_wstreamripper_prefs_structure);
    RUN_TEST(test_riplist_len_constant);
    RUN_TEST(test_wstreamripper_prefs_riplist);
    RUN_TEST(test_max_constants);
    RUN_TEST(test_opt_flag_constants);
    RUN_TEST(test_stream_prefs_http_wav_fields);

    return UNITY_END();
}
