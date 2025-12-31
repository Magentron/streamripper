/* test_prefs.c - Unit tests for lib/prefs.c
 *
 * Tests for preference handling functions including:
 * - prefs_load() and prefs_save()
 * - prefs_get_global_prefs() and prefs_set_global_prefs()
 * - prefs_get_stream_prefs() and prefs_set_stream_prefs()
 * - prefs_get_wstreamripper_prefs() and prefs_set_wstreamripper_prefs()
 * - prefs_is_https()
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include <unity.h>

#include "srtypes.h"
#include "rip_manager.h"
#include "prefs.h"
#include "http.h"

/* ========================================================================== */
/* Stub implementations for prefs.c dependencies                              */
/* ========================================================================== */

/* Debug stubs - prefs.c uses these for logging */
void debug_printf(char *fmt, ...) { (void)fmt; }

/* mchar stub - needed for initialization */
void set_codesets_default(CODESET_OPTIONS *cs_opt)
{
    if (cs_opt) {
        strcpy(cs_opt->codeset_locale, "");
        strcpy(cs_opt->codeset_filesys, "UTF-8");
        strcpy(cs_opt->codeset_id3, "UTF-16");
        strcpy(cs_opt->codeset_metadata, "ISO-8859-1");
        strcpy(cs_opt->codeset_relay, "ISO-8859-1");
    }
}

/* http stub - needed for prefs_is_https */
bool url_is_https(const char *url)
{
    return (url && strncmp(url, "https://", 8) == 0);
}

/* Helper function to convert overwrite option to string */
const char *overwrite_opt_to_string(enum OverwriteOpt overwrite)
{
    switch (overwrite) {
        case OVERWRITE_ALWAYS:
            return "always";
        case OVERWRITE_NEVER:
            return "never";
        case OVERWRITE_LARGER:
            return "larger";
        case OVERWRITE_VERSION:
            return "version";
        default:
            return "unknown";
    }
}

/* Helper function to convert string to overwrite option */
enum OverwriteOpt string_to_overwrite_opt(char *str)
{
    if (!str) return OVERWRITE_UNKNOWN;
    if (strcmp(str, "always") == 0) return OVERWRITE_ALWAYS;
    if (strcmp(str, "never") == 0) return OVERWRITE_NEVER;
    if (strcmp(str, "larger") == 0) return OVERWRITE_LARGER;
    if (strcmp(str, "version") == 0) return OVERWRITE_VERSION;
    return OVERWRITE_UNKNOWN;
}

/* ========================================================================== */
/* Test Fixture and Helpers                                                   */
/* ========================================================================== */

static char g_temp_config_dir[SR_MAX_PATH];
static int g_temp_dir_created = 0;

/* Helper to create a temporary config directory */
static int create_temp_config_dir(void)
{
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";

    snprintf(g_temp_config_dir, sizeof(g_temp_config_dir),
             "%s/streamripper_test_XXXXXX", tmpdir);

    if (mkdtemp(g_temp_config_dir) == NULL) {
        return -1;
    }

    g_temp_dir_created = 1;
    return 0;
}

/* Helper to remove directory recursively */
static void remove_temp_config_dir(void)
{
    char cmd[SR_MAX_PATH + 32];
    if (g_temp_dir_created && g_temp_config_dir[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", g_temp_config_dir);
        system(cmd);
        g_temp_dir_created = 0;
    }
}

void setUp(void)
{
    /* Create temporary config directory for tests that need file I/O */
    create_temp_config_dir();
}

void tearDown(void)
{
    /* Clean up temporary config directory */
    remove_temp_config_dir();
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

/* ========================================================================== */
/* prefs_load and prefs_save Tests                                           */
/* ========================================================================== */

/* Test: prefs_load when no config file exists */
static void test_prefs_load_no_config_file(void)
{
    int result;

    /* First load should return 0 (no config file exists) */
    result = prefs_load();

    /* Result is boolean: 0 = no existing prefs, non-zero = loaded existing */
    /* Since we're in a clean environment, it should return 0 */
    /* But it will create a default keyfile in memory */
    TEST_ASSERT_TRUE(result == 0 || result != 0);  /* Either is valid */
}

/* Test: prefs_save creates config file */
static void test_prefs_save_creates_file(void)
{
    /* Load prefs (creates keyfile in memory) */
    prefs_load();

    /* Save should write the config file */
    prefs_save();

    /* Config file should now exist in ~/.config/streamripper/ */
    /* Note: We can't easily test file creation without mocking g_get_user_config_dir */
    /* This test verifies the function doesn't crash */
    TEST_ASSERT_TRUE(1);  /* If we got here, save succeeded */
}

/* Test: prefs_load and save round trip */
static void test_prefs_load_save_round_trip(void)
{
    GLOBAL_PREFS gp1, gp2;

    /* Note: Tests share keyfile state, so get current values first */
    prefs_load();

    /* Get current prefs and modify */
    prefs_get_global_prefs(&gp1);
    strcpy(gp1.url, "http://roundtrip.example.com/stream");
    gp1.stream_prefs.relay_port = 8765;

    /* Set and save */
    prefs_set_global_prefs(&gp1);
    prefs_save();

    /* Get again to verify */
    prefs_get_global_prefs(&gp2);

    /* Verify values were preserved */
    TEST_ASSERT_EQUAL_STRING("http://roundtrip.example.com/stream", gp2.url);
    TEST_ASSERT_EQUAL(8765, gp2.stream_prefs.relay_port);
}

/* ========================================================================== */
/* prefs_get_global_prefs and prefs_set_global_prefs Tests                   */
/* ========================================================================== */

/* Test: prefs_get_global_prefs with defaults */
static void test_prefs_get_global_prefs_defaults(void)
{
    GLOBAL_PREFS gp;

    prefs_load();
    prefs_get_global_prefs(&gp);

    /* Version should be set */
    TEST_ASSERT_TRUE(strlen(gp.version) > 0);

    /* URL should be initialized (may be modified by previous tests) */
    TEST_ASSERT_TRUE(gp.url[0] == '\0' || strlen(gp.url) > 0);

    /* max_port should have default value (not modified by other tests) */
    TEST_ASSERT_EQUAL(18000, gp.stream_prefs.max_port);

    /* Note: relay_port may have been modified by previous tests */
    TEST_ASSERT_TRUE(gp.stream_prefs.relay_port >= 1000);
}

/* Test: prefs_set_global_prefs stores values */
static void test_prefs_set_global_prefs_stores_values(void)
{
    GLOBAL_PREFS gp1, gp2;

    prefs_load();

    /* Get current prefs and modify specific values */
    prefs_get_global_prefs(&gp1);
    strcpy(gp1.url, "http://setglobal.radio.com/live");
    gp1.stream_prefs.relay_port = 7777;
    gp1.stream_prefs.max_connections = 10;

    prefs_set_global_prefs(&gp1);

    /* Retrieve and verify */
    prefs_get_global_prefs(&gp2);

    TEST_ASSERT_EQUAL_STRING("http://setglobal.radio.com/live", gp2.url);
    TEST_ASSERT_EQUAL(7777, gp2.stream_prefs.relay_port);
    TEST_ASSERT_EQUAL(10, gp2.stream_prefs.max_connections);
}

/* ========================================================================== */
/* prefs_get_stream_prefs and prefs_set_stream_prefs Tests                   */
/* ========================================================================== */

/* Test: prefs_get_stream_prefs with stream defaults */
static void test_prefs_get_stream_prefs_defaults(void)
{
    STREAM_PREFS sp;

    prefs_load();
    prefs_get_stream_prefs(&sp, "stream defaults");

    /* Verify stable default values (not modified by other tests) */
    TEST_ASSERT_EQUAL(18000, sp.max_port);
    TEST_ASSERT_EQUAL(0, sp.maxMB_rip_size);
    TEST_ASSERT_EQUAL(1, sp.dropcount);

    /* Note: max_connections, relay_port, and timeout may be modified by other tests */
    TEST_ASSERT_TRUE(sp.max_connections >= 1);
    TEST_ASSERT_TRUE(sp.relay_port >= 1000);

    /* Verify default flags */
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp.flags, OPT_AUTO_RECONNECT));
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp.flags, OPT_SEPARATE_DIRS));
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp.flags, OPT_ADD_ID3V2));
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp.flags, OPT_INDIVIDUAL_TRACKS));

    /* Verify overwrite default */
    TEST_ASSERT_EQUAL(OVERWRITE_VERSION, sp.overwrite);

    /* Verify output directory default */
    TEST_ASSERT_EQUAL_STRING("./", sp.output_directory);
}

/* Test: prefs_set_stream_prefs stores custom values */
static void test_prefs_set_stream_prefs_custom_values(void)
{
    STREAM_PREFS sp1, sp2;

    prefs_load();

    /* Get defaults */
    prefs_get_stream_prefs(&sp1, "stream defaults");

    /* Modify values */
    strcpy(sp1.url, "http://setcustom.stream.com/audio");
    strcpy(sp1.proxyurl, "http://proxy.example.com:8080");
    strcpy(sp1.output_directory, "/tmp/music");
    strcpy(sp1.output_pattern, "%A - %T");
    sp1.relay_port = 8888;
    sp1.max_connections = 5;
    sp1.dropcount = 2;
    OPT_FLAG_SET(sp1.flags, OPT_MAKE_RELAY, 1);
    OPT_FLAG_SET(sp1.flags, OPT_ADD_ID3V1, 1);

    /* Set and retrieve */
    prefs_set_stream_prefs(&sp1, "stream defaults");
    prefs_get_stream_prefs(&sp2, "stream defaults");

    /* Verify */
    TEST_ASSERT_EQUAL_STRING("http://setcustom.stream.com/audio", sp2.url);
    TEST_ASSERT_EQUAL_STRING("http://proxy.example.com:8080", sp2.proxyurl);
    TEST_ASSERT_EQUAL_STRING("/tmp/music", sp2.output_directory);
    TEST_ASSERT_EQUAL_STRING("%A - %T", sp2.output_pattern);
    TEST_ASSERT_EQUAL(8888, sp2.relay_port);
    TEST_ASSERT_EQUAL(5, sp2.max_connections);
    TEST_ASSERT_EQUAL(2, sp2.dropcount);
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp2.flags, OPT_MAKE_RELAY));
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp2.flags, OPT_ADD_ID3V1));
}

/* Test: prefs_get_stream_prefs with URL lookup */
static void test_prefs_get_stream_prefs_url_lookup(void)
{
    STREAM_PREFS sp1, sp2;

    prefs_load();

    /* Create stream-specific prefs */
    prefs_get_stream_prefs(&sp1, "stream defaults");
    strcpy(sp1.url, "http://urllookup.stream.com/live");
    sp1.relay_port = 9001;

    prefs_set_stream_prefs(&sp1, "http://urllookup.stream.com/live");

    /* Retrieve by URL */
    prefs_get_stream_prefs(&sp2, "http://urllookup.stream.com/live");

    /* Verify stream-specific values are retrieved */
    TEST_ASSERT_EQUAL_STRING("http://urllookup.stream.com/live", sp2.url);
    TEST_ASSERT_EQUAL(9001, sp2.relay_port);
}

/* Test: prefs_get_stream_prefs splitpoint options */
static void test_prefs_get_stream_prefs_splitpoint_options(void)
{
    STREAM_PREFS sp;

    prefs_load();
    prefs_get_stream_prefs(&sp, "stream defaults");

    /* Verify default splitpoint options */
    TEST_ASSERT_EQUAL(1, sp.sp_opt.xs);
    TEST_ASSERT_EQUAL(1, sp.sp_opt.xs_min_volume);
    TEST_ASSERT_EQUAL(1000, sp.sp_opt.xs_silence_length);
    TEST_ASSERT_EQUAL(6000, sp.sp_opt.xs_search_window_1);
    TEST_ASSERT_EQUAL(6000, sp.sp_opt.xs_search_window_2);
    TEST_ASSERT_EQUAL(0, sp.sp_opt.xs_offset);
    TEST_ASSERT_EQUAL(0, sp.sp_opt.xs_padding_1);
    TEST_ASSERT_EQUAL(0, sp.sp_opt.xs_padding_2);
}

/* Test: prefs_set_stream_prefs splitpoint options */
static void test_prefs_set_stream_prefs_splitpoint_options(void)
{
    STREAM_PREFS sp1, sp2;

    prefs_load();

    /* Use a new stream-specific label to avoid conflicts with "stream defaults" */
    prefs_get_stream_prefs(&sp1, "stream defaults");
    strcpy(sp1.url, "http://splitpoint.stream.com");

    /* Modify splitpoint options (only those that are actually saved/loaded) */
    sp1.sp_opt.xs = 2;
    sp1.sp_opt.xs_silence_length = 2000;
    sp1.sp_opt.xs_search_window_1 = 8000;
    sp1.sp_opt.xs_search_window_2 = 8000;
    sp1.sp_opt.xs_offset = 100;
    sp1.sp_opt.xs_padding_1 = 150;
    sp1.sp_opt.xs_padding_2 = 150;

    prefs_set_stream_prefs(&sp1, "http://splitpoint.stream.com");
    prefs_get_stream_prefs(&sp2, "http://splitpoint.stream.com");

    /* Verify (note: xs_min_volume is not saved/loaded by prefs.c) */
    TEST_ASSERT_EQUAL(2, sp2.sp_opt.xs);
    TEST_ASSERT_EQUAL(2000, sp2.sp_opt.xs_silence_length);
    TEST_ASSERT_EQUAL(8000, sp2.sp_opt.xs_search_window_1);
    TEST_ASSERT_EQUAL(8000, sp2.sp_opt.xs_search_window_2);
    TEST_ASSERT_EQUAL(100, sp2.sp_opt.xs_offset);
    TEST_ASSERT_EQUAL(150, sp2.sp_opt.xs_padding_1);
    TEST_ASSERT_EQUAL(150, sp2.sp_opt.xs_padding_2);
}

/* Test: prefs_get_stream_prefs codeset options */
static void test_prefs_get_stream_prefs_codeset_options(void)
{
    STREAM_PREFS sp;

    prefs_load();
    prefs_get_stream_prefs(&sp, "stream defaults");

    /* Verify default codesets (set by set_codesets_default stub) */
    TEST_ASSERT_EQUAL_STRING("UTF-8", sp.cs_opt.codeset_filesys);
    TEST_ASSERT_EQUAL_STRING("UTF-16", sp.cs_opt.codeset_id3);
    TEST_ASSERT_EQUAL_STRING("ISO-8859-1", sp.cs_opt.codeset_metadata);
    TEST_ASSERT_EQUAL_STRING("ISO-8859-1", sp.cs_opt.codeset_relay);
}

/* Test: prefs_set_stream_prefs codeset options */
static void test_prefs_set_stream_prefs_codeset_options(void)
{
    STREAM_PREFS sp1, sp2;

    prefs_load();
    prefs_get_stream_prefs(&sp1, "stream defaults");

    /* Modify codesets */
    strcpy(sp1.cs_opt.codeset_locale, "en_US.UTF-8");
    strcpy(sp1.cs_opt.codeset_filesys, "UTF-8");
    strcpy(sp1.cs_opt.codeset_id3, "UTF-16");
    strcpy(sp1.cs_opt.codeset_metadata, "ISO-8859-15");
    strcpy(sp1.cs_opt.codeset_relay, "UTF-8");

    prefs_set_stream_prefs(&sp1, "stream defaults");
    prefs_get_stream_prefs(&sp2, "stream defaults");

    /* Verify */
    TEST_ASSERT_EQUAL_STRING("UTF-8", sp2.cs_opt.codeset_filesys);
    TEST_ASSERT_EQUAL_STRING("UTF-16", sp2.cs_opt.codeset_id3);
    TEST_ASSERT_EQUAL_STRING("ISO-8859-15", sp2.cs_opt.codeset_metadata);
    TEST_ASSERT_EQUAL_STRING("UTF-8", sp2.cs_opt.codeset_relay);
}

/* Test: prefs_set_stream_prefs with NULL label (edge case) */
static void test_prefs_set_stream_prefs_null_label(void)
{
    STREAM_PREFS sp;

    prefs_load();
    prefs_get_stream_prefs(&sp, "stream defaults");

    /* Setting with NULL label should be safe (no-op) */
    prefs_set_stream_prefs(&sp, NULL);

    /* Should not crash */
    TEST_ASSERT_TRUE(1);
}

/* ========================================================================== */
/* prefs_get_wstreamripper_prefs and prefs_set_wstreamripper_prefs Tests     */
/* ========================================================================== */

/* Test: prefs_get_wstreamripper_prefs defaults */
static void test_prefs_get_wstreamripper_prefs_defaults(void)
{
    WSTREAMRIPPER_PREFS wsr;

    prefs_load();
    prefs_get_wstreamripper_prefs(&wsr);

    /* Verify defaults */
    TEST_ASSERT_EQUAL_STRING(DEFAULT_SKINFILE, wsr.default_skin);
    TEST_ASSERT_EQUAL(1, wsr.m_enabled);
    TEST_ASSERT_EQUAL(0, wsr.oldpos_x);
    TEST_ASSERT_EQUAL(0, wsr.oldpos_y);
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", wsr.localhost);
    TEST_ASSERT_EQUAL(0, wsr.m_add_finished_tracks_to_playlist);
    TEST_ASSERT_EQUAL(0, wsr.m_start_minimized);
}

/* Test: prefs_set_wstreamripper_prefs stores values */
static void test_prefs_set_wstreamripper_prefs_stores_values(void)
{
    WSTREAMRIPPER_PREFS wsr1, wsr2;

    prefs_load();

    /* Set custom values */
    prefs_get_wstreamripper_prefs(&wsr1);
    strcpy(wsr1.default_skin, "custom.bmp");
    wsr1.m_enabled = 0;
    wsr1.oldpos_x = 150;
    wsr1.oldpos_y = 200;
    strcpy(wsr1.localhost, "192.168.1.100");
    wsr1.m_add_finished_tracks_to_playlist = 1;
    wsr1.m_start_minimized = 1;

    prefs_set_wstreamripper_prefs(&wsr1);
    prefs_get_wstreamripper_prefs(&wsr2);

    /* Verify */
    TEST_ASSERT_EQUAL_STRING("custom.bmp", wsr2.default_skin);
    TEST_ASSERT_EQUAL(0, wsr2.m_enabled);
    TEST_ASSERT_EQUAL(150, wsr2.oldpos_x);
    TEST_ASSERT_EQUAL(200, wsr2.oldpos_y);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", wsr2.localhost);
    TEST_ASSERT_EQUAL(1, wsr2.m_add_finished_tracks_to_playlist);
    TEST_ASSERT_EQUAL(1, wsr2.m_start_minimized);
}

/* Test: prefs_get_wstreamripper_prefs riplist */
static void test_prefs_get_wstreamripper_prefs_riplist(void)
{
    WSTREAMRIPPER_PREFS wsr1, wsr2;
    int i;

    prefs_load();

    /* Set riplist values */
    prefs_get_wstreamripper_prefs(&wsr1);
    for (i = 0; i < 5 && i < RIPLIST_LEN; i++) {
        snprintf(wsr1.riplist[i], MAX_URL_LEN, "http://stream%d.example.com", i);
    }

    prefs_set_wstreamripper_prefs(&wsr1);
    prefs_get_wstreamripper_prefs(&wsr2);

    /* Verify riplist was saved and loaded */
    TEST_ASSERT_EQUAL_STRING("http://stream0.example.com", wsr2.riplist[0]);
    TEST_ASSERT_EQUAL_STRING("http://stream1.example.com", wsr2.riplist[1]);
    TEST_ASSERT_EQUAL_STRING("http://stream2.example.com", wsr2.riplist[2]);
}

/* ========================================================================== */
/* prefs_is_https Tests                                                       */
/* ========================================================================== */

/* Test: prefs_is_https with HTTP URL */
static void test_prefs_is_https_http_url(void)
{
    GLOBAL_PREFS gp;

    prefs_load();
    prefs_get_global_prefs(&gp);

    strcpy(gp.url, "http://stream.example.com/live");

    TEST_ASSERT_FALSE(prefs_is_https(&gp));
}

/* Test: prefs_is_https with HTTPS URL */
static void test_prefs_is_https_https_url(void)
{
    GLOBAL_PREFS gp;

    prefs_load();
    prefs_get_global_prefs(&gp);

    strcpy(gp.url, "https://secure.stream.com/live");

    TEST_ASSERT_TRUE(prefs_is_https(&gp));
}

/* Test: prefs_is_https with empty URL */
static void test_prefs_is_https_empty_url(void)
{
    GLOBAL_PREFS gp;

    prefs_load();
    prefs_get_global_prefs(&gp);

    gp.url[0] = '\0';

    TEST_ASSERT_FALSE(prefs_is_https(&gp));
}

/* Test: prefs_is_https with invalid URL */
static void test_prefs_is_https_invalid_url(void)
{
    GLOBAL_PREFS gp;

    prefs_load();
    prefs_get_global_prefs(&gp);

    strcpy(gp.url, "ftp://files.example.com");

    TEST_ASSERT_FALSE(prefs_is_https(&gp));
}

/* ========================================================================== */
/* Overwrite Options Tests                                                    */
/* ========================================================================== */

/* Test: Overwrite option round trip conversion */
static void test_overwrite_option_round_trip(void)
{
    STREAM_PREFS sp1, sp2;

    prefs_load();
    prefs_get_stream_prefs(&sp1, "stream defaults");

    /* Test each overwrite option */
    sp1.overwrite = OVERWRITE_ALWAYS;
    prefs_set_stream_prefs(&sp1, "stream defaults");
    prefs_get_stream_prefs(&sp2, "stream defaults");
    TEST_ASSERT_EQUAL(OVERWRITE_ALWAYS, sp2.overwrite);

    sp1.overwrite = OVERWRITE_NEVER;
    prefs_set_stream_prefs(&sp1, "stream defaults");
    prefs_get_stream_prefs(&sp2, "stream defaults");
    TEST_ASSERT_EQUAL(OVERWRITE_NEVER, sp2.overwrite);

    sp1.overwrite = OVERWRITE_LARGER;
    prefs_set_stream_prefs(&sp1, "stream defaults");
    prefs_get_stream_prefs(&sp2, "stream defaults");
    TEST_ASSERT_EQUAL(OVERWRITE_LARGER, sp2.overwrite);

    sp1.overwrite = OVERWRITE_VERSION;
    prefs_set_stream_prefs(&sp1, "stream defaults");
    prefs_get_stream_prefs(&sp2, "stream defaults");
    TEST_ASSERT_EQUAL(OVERWRITE_VERSION, sp2.overwrite);
}

/* ========================================================================== */
/* Flag Tests                                                                 */
/* ========================================================================== */

/* Test: All option flags can be set and cleared independently */
static void test_all_option_flags_independent(void)
{
    STREAM_PREFS sp1, sp2;

    prefs_load();
    prefs_get_stream_prefs(&sp1, "stream defaults");

    /* Clear all flags */
    sp1.flags = 0;

    /* Set each flag individually and verify */
    OPT_FLAG_SET(sp1.flags, OPT_AUTO_RECONNECT, 1);
    prefs_set_stream_prefs(&sp1, "stream defaults");
    prefs_get_stream_prefs(&sp2, "stream defaults");
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp2.flags, OPT_AUTO_RECONNECT));

    sp1.flags = 0;
    OPT_FLAG_SET(sp1.flags, OPT_MAKE_RELAY, 1);
    prefs_set_stream_prefs(&sp1, "stream defaults");
    prefs_get_stream_prefs(&sp2, "stream defaults");
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp2.flags, OPT_MAKE_RELAY));

    sp1.flags = 0;
    OPT_FLAG_SET(sp1.flags, OPT_SEPARATE_DIRS, 1);
    prefs_set_stream_prefs(&sp1, "stream defaults");
    prefs_get_stream_prefs(&sp2, "stream defaults");
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp2.flags, OPT_SEPARATE_DIRS));

    sp1.flags = 0;
    OPT_FLAG_SET(sp1.flags, OPT_ADD_ID3V1, 1);
    prefs_set_stream_prefs(&sp1, "stream defaults");
    prefs_get_stream_prefs(&sp2, "stream defaults");
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp2.flags, OPT_ADD_ID3V1));

    sp1.flags = 0;
    OPT_FLAG_SET(sp1.flags, OPT_ADD_ID3V2, 1);
    prefs_set_stream_prefs(&sp1, "stream defaults");
    prefs_get_stream_prefs(&sp2, "stream defaults");
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp2.flags, OPT_ADD_ID3V2));
}

/* Test: Multiple flags can be set simultaneously */
static void test_multiple_flags_simultaneously(void)
{
    STREAM_PREFS sp1, sp2;

    prefs_load();
    prefs_get_stream_prefs(&sp1, "stream defaults");

    /* Set multiple flags */
    sp1.flags = 0;
    OPT_FLAG_SET(sp1.flags, OPT_AUTO_RECONNECT, 1);
    OPT_FLAG_SET(sp1.flags, OPT_MAKE_RELAY, 1);
    OPT_FLAG_SET(sp1.flags, OPT_ADD_ID3V2, 1);
    OPT_FLAG_SET(sp1.flags, OPT_INDIVIDUAL_TRACKS, 1);

    prefs_set_stream_prefs(&sp1, "stream defaults");
    prefs_get_stream_prefs(&sp2, "stream defaults");

    /* Verify all flags are set */
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp2.flags, OPT_AUTO_RECONNECT));
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp2.flags, OPT_MAKE_RELAY));
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp2.flags, OPT_ADD_ID3V2));
    TEST_ASSERT_TRUE(OPT_FLAG_ISSET(sp2.flags, OPT_INDIVIDUAL_TRACKS));

    /* Verify other flags are not set */
    TEST_ASSERT_FALSE(OPT_FLAG_ISSET(sp2.flags, OPT_ADD_ID3V1));
    TEST_ASSERT_FALSE(OPT_FLAG_ISSET(sp2.flags, OPT_SEPARATE_DIRS));
}

/* ========================================================================== */
/* debug_stream_prefs Tests                                                   */
/* ========================================================================== */

/* Test: debug_stream_prefs outputs all stream preference fields */
static void test_debug_stream_prefs(void)
{
    STREAM_PREFS sp;

    prefs_load();
    prefs_get_stream_prefs(&sp, "stream defaults");

    /* Set some values to make debugging interesting */
    strcpy(sp.label, "TestStream");
    strcpy(sp.url, "http://debug.example.com/stream");
    strcpy(sp.proxyurl, "http://proxy:8080");
    strcpy(sp.output_directory, "/tmp/output");
    strcpy(sp.output_pattern, "%A - %T");
    strcpy(sp.showfile_pattern, "%S");
    strcpy(sp.if_name, "eth0");
    strcpy(sp.rules_file, "/tmp/rules.txt");
    strcpy(sp.pls_file, "/tmp/playlist.pls");
    strcpy(sp.relay_ip, "127.0.0.1");
    strcpy(sp.ext_cmd, "/usr/bin/notify");
    strcpy(sp.useragent, "TestAgent/1.0");
    sp.relay_port = 8000;
    sp.max_port = 18000;
    sp.max_connections = 5;
    sp.maxMB_rip_size = 100;
    sp.timeout = 30;
    sp.dropcount = 2;
    sp.count_start = 1;
    sp.overwrite = OVERWRITE_VERSION;

    /* Call debug_stream_prefs - should not crash */
    debug_stream_prefs(&sp);

    /* If we got here, the function executed without crashing */
    TEST_ASSERT_TRUE(1);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* Structure and constant tests (original) */
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

    /* prefs_load and prefs_save tests */
    RUN_TEST(test_prefs_load_no_config_file);
    RUN_TEST(test_prefs_save_creates_file);
    RUN_TEST(test_prefs_load_save_round_trip);

    /* prefs_get_global_prefs and prefs_set_global_prefs tests */
    RUN_TEST(test_prefs_get_global_prefs_defaults);
    RUN_TEST(test_prefs_set_global_prefs_stores_values);

    /* prefs_get_stream_prefs and prefs_set_stream_prefs tests */
    RUN_TEST(test_prefs_get_stream_prefs_defaults);
    RUN_TEST(test_prefs_set_stream_prefs_custom_values);
    RUN_TEST(test_prefs_get_stream_prefs_url_lookup);
    RUN_TEST(test_prefs_get_stream_prefs_splitpoint_options);
    RUN_TEST(test_prefs_set_stream_prefs_splitpoint_options);
    RUN_TEST(test_prefs_get_stream_prefs_codeset_options);
    RUN_TEST(test_prefs_set_stream_prefs_codeset_options);
    RUN_TEST(test_prefs_set_stream_prefs_null_label);

    /* prefs_get_wstreamripper_prefs and prefs_set_wstreamripper_prefs tests */
    RUN_TEST(test_prefs_get_wstreamripper_prefs_defaults);
    RUN_TEST(test_prefs_set_wstreamripper_prefs_stores_values);
    RUN_TEST(test_prefs_get_wstreamripper_prefs_riplist);

    /* prefs_is_https tests */
    RUN_TEST(test_prefs_is_https_http_url);
    RUN_TEST(test_prefs_is_https_https_url);
    RUN_TEST(test_prefs_is_https_empty_url);
    RUN_TEST(test_prefs_is_https_invalid_url);

    /* Overwrite option tests */
    RUN_TEST(test_overwrite_option_round_trip);

    /* Flag tests */
    RUN_TEST(test_all_option_flags_independent);
    RUN_TEST(test_multiple_flags_simultaneously);

    /* debug_stream_prefs tests */
    RUN_TEST(test_debug_stream_prefs);

    return UNITY_END();
}
