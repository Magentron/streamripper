/* test_findsep.c - Unit tests for lib/findsep.c
 *
 * Tests for silence detection related data structures and constants.
 * Note: The actual findsep functions require libmad for MP3 decoding.
 * These tests focus on data structure validation and SPLITPOINT_OPTIONS.
 */
#include <string.h>
#include <stdlib.h>
#include <unity.h>

#include "srtypes.h"
#include "errors.h"

/* MP3 frame header structure for reference/testing
 *
 * Frame sync: 0xFF 0xFB (11 sync bits = 1)
 * MPEG Version: 11 (MPEG 1)
 * Layer: 01 (Layer 3)
 * Protection bit: 1 (no CRC)
 * Bitrate index: various
 * Sampling freq: various
 */

/* Sample MP3 frame headers for different bitrates */
static const unsigned char mp3_header_128kbps[] = {
    0xFF, 0xFB, 0x90, 0x00  /* 128kbps, 44100Hz, MPEG1 Layer3 */
};

static const unsigned char mp3_header_192kbps[] = {
    0xFF, 0xFB, 0xB0, 0x00  /* 192kbps, 44100Hz, MPEG1 Layer3 */
};

static const unsigned char mp3_header_256kbps[] = {
    0xFF, 0xFB, 0xD0, 0x00  /* 256kbps, 44100Hz, MPEG1 Layer3 */
};

/* Invalid data (not a valid MP3) */
static const unsigned char invalid_mp3_data[] = {
    0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07
};

/* MPEG1 Layer3 bitrate table (index 0 is free, 1-14 are rates, 15 is bad) */
static const int mp3_bitrate_table[] = {
    0,    /* free */
    32,   /* index 1 */
    40,   /* index 2 */
    48,   /* index 3 */
    56,   /* index 4 */
    64,   /* index 5 */
    80,   /* index 6 */
    96,   /* index 7 */
    112,  /* index 8 */
    128,  /* index 9 */
    160,  /* index 10 */
    192,  /* index 11 */
    224,  /* index 12 */
    256,  /* index 13 */
    320   /* index 14 */
};

/* MPEG1 sample rate table */
static const int mp3_samplerate_table[] = {
    44100,  /* index 0 */
    48000,  /* index 1 */
    32000   /* index 2 */
};

void setUp(void)
{
    /* Nothing to set up */
}

void tearDown(void)
{
    /* Nothing to clean up */
}

/* ============================================================================
 * SPLITPOINT_OPTIONS Structure Tests
 * ============================================================================ */

/* Test: SPLITPOINT_OPTIONS structure initialization */
static void test_splitpoint_options_init(void)
{
    SPLITPOINT_OPTIONS sp;
    memset(&sp, 0, sizeof(sp));

    TEST_ASSERT_EQUAL(0, sp.xs);
    TEST_ASSERT_EQUAL(0, sp.xs_min_volume);
    TEST_ASSERT_EQUAL(0, sp.xs_silence_length);
    TEST_ASSERT_EQUAL(0, sp.xs_search_window_1);
    TEST_ASSERT_EQUAL(0, sp.xs_search_window_2);
    TEST_ASSERT_EQUAL(0, sp.xs_offset);
    TEST_ASSERT_EQUAL(0, sp.xs_padding_1);
    TEST_ASSERT_EQUAL(0, sp.xs_padding_2);
}

/* Test: SPLITPOINT_OPTIONS typical values */
static void test_splitpoint_options_typical_values(void)
{
    SPLITPOINT_OPTIONS sp;

    /* Set typical silence detection parameters */
    sp.xs = 1;                    /* Enable silence splitting */
    sp.xs_min_volume = 10;        /* Minimum volume threshold */
    sp.xs_silence_length = 1000;  /* 1 second of silence */
    sp.xs_search_window_1 = 5000; /* 5 second search window before */
    sp.xs_search_window_2 = 5000; /* 5 second search window after */
    sp.xs_offset = 0;             /* No offset */
    sp.xs_padding_1 = 100;        /* 100ms padding before */
    sp.xs_padding_2 = 100;        /* 100ms padding after */

    TEST_ASSERT_EQUAL(1, sp.xs);
    TEST_ASSERT_EQUAL(10, sp.xs_min_volume);
    TEST_ASSERT_EQUAL(1000, sp.xs_silence_length);
    TEST_ASSERT_EQUAL(5000, sp.xs_search_window_1);
    TEST_ASSERT_EQUAL(5000, sp.xs_search_window_2);
    TEST_ASSERT_EQUAL(0, sp.xs_offset);
    TEST_ASSERT_EQUAL(100, sp.xs_padding_1);
    TEST_ASSERT_EQUAL(100, sp.xs_padding_2);
}

/* Test: SPLITPOINT_OPTIONS negative padding */
static void test_splitpoint_options_negative_padding(void)
{
    SPLITPOINT_OPTIONS sp;
    memset(&sp, 0, sizeof(sp));

    /* Negative padding could be used to trim */
    sp.xs_padding_1 = -50;
    sp.xs_padding_2 = -50;

    TEST_ASSERT_EQUAL(-50, sp.xs_padding_1);
    TEST_ASSERT_EQUAL(-50, sp.xs_padding_2);
}

/* Test: SPLITPOINT_OPTIONS in STREAM_PREFS */
static void test_splitpoint_options_in_stream_prefs(void)
{
    STREAM_PREFS prefs;
    memset(&prefs, 0, sizeof(prefs));

    prefs.sp_opt.xs = 1;
    prefs.sp_opt.xs_silence_length = 500;
    prefs.sp_opt.xs_search_window_1 = 3000;
    prefs.sp_opt.xs_search_window_2 = 3000;

    TEST_ASSERT_EQUAL(1, prefs.sp_opt.xs);
    TEST_ASSERT_EQUAL(500, prefs.sp_opt.xs_silence_length);
}

/* ============================================================================
 * MP3 Frame Header Parsing Tests
 * ============================================================================ */

/* Test: MP3 sync word detection */
static void test_mp3_sync_word(void)
{
    /* MP3 sync word is 0xFF followed by 0xFx (where x has bits 7,6,5 set) */
    TEST_ASSERT_EQUAL(0xFF, mp3_header_128kbps[0]);
    TEST_ASSERT_TRUE((mp3_header_128kbps[1] & 0xE0) == 0xE0);
}

/* Test: Extract MPEG version from header */
static void test_mp3_mpeg_version(void)
{
    /* MPEG version is bits 4-3 of byte 1: 11=v1, 10=v2, 00=v2.5 */
    unsigned char version = (mp3_header_128kbps[1] >> 3) & 0x03;

    /* Our test header is MPEG1 (version = 3) */
    TEST_ASSERT_EQUAL(3, version);
}

/* Test: Extract layer from header */
static void test_mp3_layer(void)
{
    /* Layer is bits 2-1 of byte 1: 11=Layer1, 10=Layer2, 01=Layer3 */
    unsigned char layer = (mp3_header_128kbps[1] >> 1) & 0x03;

    /* Our test header is Layer 3 (layer = 1) */
    TEST_ASSERT_EQUAL(1, layer);
}

/* Test: Extract bitrate index from header */
static void test_mp3_bitrate_index(void)
{
    /* Bitrate index is bits 7-4 of byte 2 */
    unsigned char br_index_128 = (mp3_header_128kbps[2] >> 4) & 0x0F;
    unsigned char br_index_192 = (mp3_header_192kbps[2] >> 4) & 0x0F;
    unsigned char br_index_256 = (mp3_header_256kbps[2] >> 4) & 0x0F;

    /* Verify bitrate indices */
    TEST_ASSERT_EQUAL(9, br_index_128);   /* Index 9 = 128kbps */
    TEST_ASSERT_EQUAL(11, br_index_192);  /* Index 11 = 192kbps */
    TEST_ASSERT_EQUAL(13, br_index_256);  /* Index 13 = 256kbps */
}

/* Test: Bitrate table lookup */
static void test_mp3_bitrate_lookup(void)
{
    unsigned char br_index_128 = (mp3_header_128kbps[2] >> 4) & 0x0F;
    unsigned char br_index_192 = (mp3_header_192kbps[2] >> 4) & 0x0F;
    unsigned char br_index_256 = (mp3_header_256kbps[2] >> 4) & 0x0F;

    TEST_ASSERT_EQUAL(128, mp3_bitrate_table[br_index_128]);
    TEST_ASSERT_EQUAL(192, mp3_bitrate_table[br_index_192]);
    TEST_ASSERT_EQUAL(256, mp3_bitrate_table[br_index_256]);
}

/* Test: Extract sample rate index from header */
static void test_mp3_samplerate_index(void)
{
    /* Sample rate index is bits 3-2 of byte 2 */
    unsigned char sr_index = (mp3_header_128kbps[2] >> 2) & 0x03;

    /* Our test header has index 0 = 44100 Hz */
    TEST_ASSERT_EQUAL(0, sr_index);
    TEST_ASSERT_EQUAL(44100, mp3_samplerate_table[sr_index]);
}

/* Test: Invalid sync word detection */
static void test_mp3_invalid_sync(void)
{
    int is_valid = (invalid_mp3_data[0] == 0xFF) &&
                   ((invalid_mp3_data[1] & 0xE0) == 0xE0);
    TEST_ASSERT_FALSE(is_valid);
}

/* ============================================================================
 * Frame Size Calculation Tests
 * ============================================================================ */

/* Test: MPEG1 Layer 3 frame size calculation */
static void test_mp3_frame_size_calculation(void)
{
    /* MPEG1 Layer 3 frame size formula:
     * frame_size = 144 * bitrate * 1000 / sample_rate + padding
     *
     * For 128kbps, 44100Hz, no padding:
     * frame_size = 144 * 128000 / 44100 = 417.96 = 417 bytes
     */
    int bitrate = 128;
    int samplerate = 44100;
    int padding = 0;

    int frame_size = (144 * bitrate * 1000) / samplerate + padding;

    /* Verify expected frame size (approximately 417-418 bytes) */
    TEST_ASSERT_TRUE(frame_size >= 417);
    TEST_ASSERT_TRUE(frame_size <= 418);
}

/* Test: Frame size varies with bitrate */
static void test_mp3_frame_size_varies(void)
{
    int samplerate = 44100;

    int frame_128 = (144 * 128 * 1000) / samplerate;
    int frame_192 = (144 * 192 * 1000) / samplerate;
    int frame_256 = (144 * 256 * 1000) / samplerate;

    /* Higher bitrate = larger frame */
    TEST_ASSERT_TRUE(frame_128 < frame_192);
    TEST_ASSERT_TRUE(frame_192 < frame_256);
}

/* ============================================================================
 * Silence Detection Parameter Tests
 * ============================================================================ */

/* Test: Silence length in milliseconds */
static void test_silence_length_ms(void)
{
    SPLITPOINT_OPTIONS sp;

    /* Typical silence lengths */
    sp.xs_silence_length = 100;   /* 100ms - very short */
    TEST_ASSERT_EQUAL(100, sp.xs_silence_length);

    sp.xs_silence_length = 500;   /* 500ms - half second */
    TEST_ASSERT_EQUAL(500, sp.xs_silence_length);

    sp.xs_silence_length = 2000;  /* 2 seconds */
    TEST_ASSERT_EQUAL(2000, sp.xs_silence_length);
}

/* Test: Search window boundaries */
static void test_search_window_boundaries(void)
{
    SPLITPOINT_OPTIONS sp;

    /* Very small window */
    sp.xs_search_window_1 = 100;
    sp.xs_search_window_2 = 100;
    TEST_ASSERT_EQUAL(100, sp.xs_search_window_1);

    /* Large window */
    sp.xs_search_window_1 = 30000;  /* 30 seconds */
    sp.xs_search_window_2 = 30000;
    TEST_ASSERT_EQUAL(30000, sp.xs_search_window_1);
}

/* Test: Volume threshold */
static void test_volume_threshold(void)
{
    SPLITPOINT_OPTIONS sp;

    /* Low threshold - more sensitive */
    sp.xs_min_volume = 1;
    TEST_ASSERT_EQUAL(1, sp.xs_min_volume);

    /* High threshold - less sensitive */
    sp.xs_min_volume = 100;
    TEST_ASSERT_EQUAL(100, sp.xs_min_volume);
}

/* ============================================================================
 * RIP_MANAGER_INFO Fields Tests
 * ============================================================================ */

/* Test: RIP_MANAGER_INFO has find_silence field */
static void test_rmi_find_silence_field(void)
{
    RIP_MANAGER_INFO rmi;
    memset(&rmi, 0, sizeof(rmi));

    /* find_silence tracks blocks before silence detection */
    rmi.find_silence = 0;
    TEST_ASSERT_EQUAL(0, rmi.find_silence);

    rmi.find_silence = 10;
    TEST_ASSERT_EQUAL(10, rmi.find_silence);
}

/* Test: RIP_MANAGER_INFO has detected_bitrate field */
static void test_rmi_detected_bitrate_field(void)
{
    RIP_MANAGER_INFO rmi;
    memset(&rmi, 0, sizeof(rmi));

    rmi.detected_bitrate = 128;
    TEST_ASSERT_EQUAL(128, rmi.detected_bitrate);

    rmi.detected_bitrate = 320;
    TEST_ASSERT_EQUAL(320, rmi.detected_bitrate);
}

/* Test: RIP_MANAGER_INFO has http_bitrate field */
static void test_rmi_http_bitrate_field(void)
{
    RIP_MANAGER_INFO rmi;
    memset(&rmi, 0, sizeof(rmi));

    /* Bitrate from HTTP header */
    rmi.http_bitrate = 128;
    TEST_ASSERT_EQUAL(128, rmi.http_bitrate);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* SPLITPOINT_OPTIONS Structure Tests */
    RUN_TEST(test_splitpoint_options_init);
    RUN_TEST(test_splitpoint_options_typical_values);
    RUN_TEST(test_splitpoint_options_negative_padding);
    RUN_TEST(test_splitpoint_options_in_stream_prefs);

    /* MP3 Frame Header Parsing Tests */
    RUN_TEST(test_mp3_sync_word);
    RUN_TEST(test_mp3_mpeg_version);
    RUN_TEST(test_mp3_layer);
    RUN_TEST(test_mp3_bitrate_index);
    RUN_TEST(test_mp3_bitrate_lookup);
    RUN_TEST(test_mp3_samplerate_index);
    RUN_TEST(test_mp3_invalid_sync);

    /* Frame Size Calculation Tests */
    RUN_TEST(test_mp3_frame_size_calculation);
    RUN_TEST(test_mp3_frame_size_varies);

    /* Silence Detection Parameter Tests */
    RUN_TEST(test_silence_length_ms);
    RUN_TEST(test_search_window_boundaries);
    RUN_TEST(test_volume_threshold);

    /* RIP_MANAGER_INFO Fields Tests */
    RUN_TEST(test_rmi_find_silence_field);
    RUN_TEST(test_rmi_detected_bitrate_field);
    RUN_TEST(test_rmi_http_bitrate_field);

    return UNITY_END();
}
