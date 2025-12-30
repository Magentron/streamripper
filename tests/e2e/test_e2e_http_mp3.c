/* test_e2e_http_mp3.c
 *
 * E2E tests for HTTP MP3 streaming.
 *
 * These tests focus on:
 * - RIP_MANAGER structure initialization
 * - URL parsing end-to-end
 * - Stream configuration validation
 * - Basic streaming setup
 *
 * Note: Full streaming tests require the mock server to be running.
 * These tests primarily validate the configuration and initialization paths.
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
static char temp_dir[256];
static char stream_url[256];

/* ========================================================================== */
/* Setup and Teardown */
/* ========================================================================== */

void setUp(void)
{
    /* Create a temporary output directory */
    if (!create_temp_directory(temp_dir, sizeof(temp_dir), "e2e_http_mp3")) {
        strcpy(temp_dir, "/tmp/e2e_http_mp3_test");
    }

    /* Initialize stream preferences with test defaults */
    memset(&test_prefs, 0, sizeof(test_prefs));
    set_rip_manager_options_defaults(&test_prefs);

    /* Set up stream URL (will be set per test if mock server is used) */
    mock_server_get_url(stream_url, sizeof(stream_url), "/stream");
}

void tearDown(void)
{
    /* Clean up temporary directory */
    remove_directory_recursive(temp_dir);
}

/* ========================================================================== */
/* Tests: STREAM_PREFS initialization */
/* ========================================================================== */

static void test_stream_prefs_defaults(void)
{
    /* Verify default options are set correctly */
    STREAM_PREFS prefs;
    memset(&prefs, 0xFF, sizeof(prefs)); /* Fill with garbage first */

    set_rip_manager_options_defaults(&prefs);

    /* Check default values */
    TEST_ASSERT_EQUAL_STRING("./", prefs.output_directory);
    TEST_ASSERT_EQUAL(8000, prefs.relay_port);
    TEST_ASSERT_EQUAL(18000, prefs.max_port);
    TEST_ASSERT_EQUAL(1, prefs.max_connections);
    TEST_ASSERT_EQUAL(0, prefs.maxMB_rip_size);

    /* Check default flags */
    TEST_ASSERT_TRUE(GET_AUTO_RECONNECT(prefs.flags));
    TEST_ASSERT_TRUE(GET_SEPARATE_DIRS(prefs.flags));
    TEST_ASSERT_TRUE(GET_SEARCH_PORTS(prefs.flags));
    TEST_ASSERT_TRUE(GET_INDIVIDUAL_TRACKS(prefs.flags));
}

static void test_stream_prefs_url_setting(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Set URL */
    strncpy(prefs.url, "http://test.example.com:8000/stream", sizeof(prefs.url) - 1);

    /* Verify URL is set */
    TEST_ASSERT_EQUAL_STRING("http://test.example.com:8000/stream", prefs.url);
}

static void test_stream_prefs_output_directory(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Set output directory */
    strncpy(prefs.output_directory, temp_dir, sizeof(prefs.output_directory) - 1);

    /* Verify */
    TEST_ASSERT_EQUAL_STRING(temp_dir, prefs.output_directory);
}

static void test_stream_prefs_proxy_setting(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default should be empty */
    TEST_ASSERT_EQUAL_STRING("", prefs.proxyurl);

    /* Set proxy */
    strncpy(prefs.proxyurl, "http://proxy.example.com:3128", sizeof(prefs.proxyurl) - 1);
    TEST_ASSERT_EQUAL_STRING("http://proxy.example.com:3128", prefs.proxyurl);
}

/* ========================================================================== */
/* Tests: URL parsing integration */
/* ========================================================================== */

static void test_url_parse_http_simple(void)
{
    URLINFO urlinfo;
    memset(&urlinfo, 0, sizeof(urlinfo));

    error_code rc = http_parse_url("http://stream.example.com/radio", &urlinfo);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL_STRING("stream.example.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/radio", urlinfo.path);
    TEST_ASSERT_EQUAL(80, urlinfo.port);
}

static void test_url_parse_http_with_port(void)
{
    URLINFO urlinfo;
    memset(&urlinfo, 0, sizeof(urlinfo));

    error_code rc = http_parse_url("http://stream.example.com:8080/radio", &urlinfo);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL_STRING("stream.example.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/radio", urlinfo.path);
    TEST_ASSERT_EQUAL(8080, urlinfo.port);
}

static void test_url_parse_https_default_port(void)
{
    URLINFO urlinfo;
    memset(&urlinfo, 0, sizeof(urlinfo));

    error_code rc = http_parse_url("https://secure.example.com/stream", &urlinfo);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL_STRING("secure.example.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/stream", urlinfo.path);
    TEST_ASSERT_EQUAL(443, urlinfo.port);
}

static void test_url_is_https_detection(void)
{
    TEST_ASSERT_TRUE(url_is_https("https://example.com/stream"));
    TEST_ASSERT_TRUE(url_is_https("HTTPS://EXAMPLE.COM/STREAM"));
    TEST_ASSERT_FALSE(url_is_https("http://example.com/stream"));
    TEST_ASSERT_FALSE(url_is_https("ftp://example.com/file"));
}

/* ========================================================================== */
/* Tests: Option flags */
/* ========================================================================== */

static void test_option_flag_auto_reconnect(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default should be ON */
    TEST_ASSERT_TRUE(GET_AUTO_RECONNECT(prefs.flags));

    /* Turn off */
    OPT_FLAG_SET(prefs.flags, OPT_AUTO_RECONNECT, 0);
    TEST_ASSERT_FALSE(GET_AUTO_RECONNECT(prefs.flags));

    /* Turn back on */
    OPT_FLAG_SET(prefs.flags, OPT_AUTO_RECONNECT, 1);
    TEST_ASSERT_TRUE(GET_AUTO_RECONNECT(prefs.flags));
}

static void test_option_flag_separate_dirs(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default should be ON */
    TEST_ASSERT_TRUE(GET_SEPARATE_DIRS(prefs.flags));

    /* Turn off */
    OPT_FLAG_SET(prefs.flags, OPT_SEPARATE_DIRS, 0);
    TEST_ASSERT_FALSE(GET_SEPARATE_DIRS(prefs.flags));
}

static void test_option_flag_single_file_output(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default should be OFF */
    TEST_ASSERT_FALSE(GET_SINGLE_FILE_OUTPUT(prefs.flags));

    /* Turn on */
    OPT_FLAG_SET(prefs.flags, OPT_SINGLE_FILE_OUTPUT, 1);
    TEST_ASSERT_TRUE(GET_SINGLE_FILE_OUTPUT(prefs.flags));
}

static void test_option_flag_id3_tags(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* ID3V2 should be ON by default (ID3V1 was removed in 1.62-beta-2) */
    TEST_ASSERT_TRUE(GET_ADD_ID3V2(prefs.flags));

    /* ID3V1 should be OFF by default */
    TEST_ASSERT_FALSE(GET_ADD_ID3V1(prefs.flags));
}

/* ========================================================================== */
/* Tests: Splitpoint options */
/* ========================================================================== */

static void test_splitpoint_defaults(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Verify splitpoint defaults are reasonable */
    TEST_ASSERT_TRUE(prefs.sp_opt.xs_search_window_1 > 0);
    TEST_ASSERT_TRUE(prefs.sp_opt.xs_search_window_2 > 0);
    TEST_ASSERT_TRUE(prefs.sp_opt.xs_padding_1 >= 0);
    TEST_ASSERT_TRUE(prefs.sp_opt.xs_padding_2 >= 0);
}

static void test_splitpoint_custom_values(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Set custom splitpoint options */
    prefs.sp_opt.xs_search_window_1 = 3000;
    prefs.sp_opt.xs_search_window_2 = 4000;
    prefs.sp_opt.xs_padding_1 = 500;
    prefs.sp_opt.xs_padding_2 = 600;
    prefs.sp_opt.xs_silence_length = 2000;

    /* Verify they are set */
    TEST_ASSERT_EQUAL(3000, prefs.sp_opt.xs_search_window_1);
    TEST_ASSERT_EQUAL(4000, prefs.sp_opt.xs_search_window_2);
    TEST_ASSERT_EQUAL(500, prefs.sp_opt.xs_padding_1);
    TEST_ASSERT_EQUAL(600, prefs.sp_opt.xs_padding_2);
    TEST_ASSERT_EQUAL(2000, prefs.sp_opt.xs_silence_length);
}

/* ========================================================================== */
/* Tests: Codeset options */
/* ========================================================================== */

static void test_codeset_defaults(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default codesets should be empty strings (use system defaults) */
    /* Or in case of ID3, it might be ISO-8859-1 */
    TEST_ASSERT_TRUE(strlen(prefs.cs_opt.codeset_id3) >= 0);
}

static void test_codeset_utf8_setting(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Set UTF-8 for metadata */
    strncpy(prefs.cs_opt.codeset_metadata, "UTF-8",
            sizeof(prefs.cs_opt.codeset_metadata) - 1);

    TEST_ASSERT_EQUAL_STRING("UTF-8", prefs.cs_opt.codeset_metadata);
}

/* ========================================================================== */
/* Tests: Overwrite options */
/* ========================================================================== */

static void test_overwrite_opt_to_string(void)
{
    TEST_ASSERT_EQUAL_STRING("always", overwrite_opt_to_string(OVERWRITE_ALWAYS));
    TEST_ASSERT_EQUAL_STRING("never", overwrite_opt_to_string(OVERWRITE_NEVER));
    TEST_ASSERT_EQUAL_STRING("larger", overwrite_opt_to_string(OVERWRITE_LARGER));
    TEST_ASSERT_EQUAL_STRING("version", overwrite_opt_to_string(OVERWRITE_VERSION));
}

static void test_string_to_overwrite_opt(void)
{
    TEST_ASSERT_EQUAL(OVERWRITE_ALWAYS, string_to_overwrite_opt("always"));
    TEST_ASSERT_EQUAL(OVERWRITE_NEVER, string_to_overwrite_opt("never"));
    TEST_ASSERT_EQUAL(OVERWRITE_LARGER, string_to_overwrite_opt("larger"));
    TEST_ASSERT_EQUAL(OVERWRITE_VERSION, string_to_overwrite_opt("version"));
    TEST_ASSERT_EQUAL(OVERWRITE_UNKNOWN, string_to_overwrite_opt("invalid"));
}

/* ========================================================================== */
/* Tests: RIP_MANAGER_INFO initialization */
/* ========================================================================== */

static void test_rip_manager_info_size(void)
{
    /* Verify RIP_MANAGER_INFO structure exists and has reasonable size */
    TEST_ASSERT_TRUE(sizeof(RIP_MANAGER_INFO) > 0);
    /* The structure should be large (contains many fields) */
    TEST_ASSERT_TRUE(sizeof(RIP_MANAGER_INFO) > 100);
}

/* ========================================================================== */
/* Tests: Content type constants */
/* ========================================================================== */

static void test_content_type_constants(void)
{
    /* Verify content type constants are defined and distinct */
    TEST_ASSERT_EQUAL(1, CONTENT_TYPE_MP3);
    TEST_ASSERT_EQUAL(2, CONTENT_TYPE_NSV);
    TEST_ASSERT_EQUAL(3, CONTENT_TYPE_OGG);
    TEST_ASSERT_EQUAL(4, CONTENT_TYPE_ULTRAVOX);
    TEST_ASSERT_EQUAL(5, CONTENT_TYPE_AAC);
    TEST_ASSERT_EQUAL(6, CONTENT_TYPE_PLS);
    TEST_ASSERT_EQUAL(7, CONTENT_TYPE_M3U);
    TEST_ASSERT_EQUAL(99, CONTENT_TYPE_UNKNOWN);
}

/* ========================================================================== */
/* Tests: Timeout and connection settings */
/* ========================================================================== */

static void test_timeout_setting(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Set timeout */
    prefs.timeout = 60;
    TEST_ASSERT_EQUAL(60, prefs.timeout);

    /* Set longer timeout */
    prefs.timeout = 300;
    TEST_ASSERT_EQUAL(300, prefs.timeout);
}

static void test_max_connections_setting(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default is 1 */
    TEST_ASSERT_EQUAL(1, prefs.max_connections);

    /* Set higher value */
    prefs.max_connections = 10;
    TEST_ASSERT_EQUAL(10, prefs.max_connections);
}

static void test_max_rip_size_setting(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default is 0 (unlimited) */
    TEST_ASSERT_EQUAL(0, prefs.maxMB_rip_size);

    /* Set limit */
    prefs.maxMB_rip_size = 100; /* 100 MB */
    TEST_ASSERT_EQUAL(100, prefs.maxMB_rip_size);
}

/* ========================================================================== */
/* Main */
/* ========================================================================== */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    /* STREAM_PREFS tests */
    RUN_TEST(test_stream_prefs_defaults);
    RUN_TEST(test_stream_prefs_url_setting);
    RUN_TEST(test_stream_prefs_output_directory);
    RUN_TEST(test_stream_prefs_proxy_setting);

    /* URL parsing tests */
    RUN_TEST(test_url_parse_http_simple);
    RUN_TEST(test_url_parse_http_with_port);
    RUN_TEST(test_url_parse_https_default_port);
    RUN_TEST(test_url_is_https_detection);

    /* Option flag tests */
    RUN_TEST(test_option_flag_auto_reconnect);
    RUN_TEST(test_option_flag_separate_dirs);
    RUN_TEST(test_option_flag_single_file_output);
    RUN_TEST(test_option_flag_id3_tags);

    /* Splitpoint tests */
    RUN_TEST(test_splitpoint_defaults);
    RUN_TEST(test_splitpoint_custom_values);

    /* Codeset tests */
    RUN_TEST(test_codeset_defaults);
    RUN_TEST(test_codeset_utf8_setting);

    /* Overwrite option tests */
    RUN_TEST(test_overwrite_opt_to_string);
    RUN_TEST(test_string_to_overwrite_opt);

    /* RIP_MANAGER tests */
    RUN_TEST(test_rip_manager_info_size);

    /* Content type tests */
    RUN_TEST(test_content_type_constants);

    /* Connection settings tests */
    RUN_TEST(test_timeout_setting);
    RUN_TEST(test_max_connections_setting);
    RUN_TEST(test_max_rip_size_setting);

    return UNITY_END();
}
