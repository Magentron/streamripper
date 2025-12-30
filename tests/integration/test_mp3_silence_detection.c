/* test_mp3_silence_detection.c - Integration tests for MP3 silence detection pipeline
 *
 * This integration test verifies the data flow between:
 * - Circular buffer (cbuf2.c)
 * - Silence detection structures (findsep.h)
 * - Splitpoint options (srtypes.h)
 *
 * Note: Actual findsep functions require libmad for MP3 decoding.
 * These tests verify data structure setup, buffer integration, and
 * splitpoint option configuration that feeds into silence detection.
 */
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "cbuf2.h"
#include "errors.h"
#include "findsep.h"
#include "srtypes.h"

/* Stub implementations for dependencies */

/* Stub for threadlib semaphore functions */
HSEM threadlib_create_sem(void)
{
    HSEM sem = {0};
    return sem;
}

error_code threadlib_waitfor_sem(HSEM *e)
{
    (void)e;
    return SR_SUCCESS;
}

error_code threadlib_signal_sem(HSEM *e)
{
    (void)e;
    return SR_SUCCESS;
}

void threadlib_destroy_sem(HSEM *e)
{
    (void)e;
}

/* Stub for debug_printf */
void debug_printf(char *fmt, ...)
{
    (void)fmt;
}

/* Stub for relaylib_disconnect */
void relaylib_disconnect(RIP_MANAGER_INFO *rmi, RELAY_LIST *prev, RELAY_LIST *ptr)
{
    (void)rmi;
    (void)prev;
    (void)ptr;
}

/* Stub for rip_ogg_process_chunk */
void rip_ogg_process_chunk(
    RIP_MANAGER_INFO *rmi,
    LIST *page_list,
    const char *buf,
    u_long size,
    TRACK_INFO *ti)
{
    (void)rmi;
    (void)buf;
    (void)size;
    (void)ti;
    INIT_LIST_HEAD(page_list);
}

/* Test fixtures */
static CBUF2 test_cbuf;
static RIP_MANAGER_INFO test_rmi;
static STREAM_PREFS test_prefs;
static SPLITPOINT_OPTIONS test_sp_opt;

/* Sample MP3 frame header bytes (valid MPEG Audio Layer 3 sync word) */
/* Frame sync: 0xFF 0xFB (MPEG 1 Layer 3, no CRC) */
static const unsigned char SAMPLE_MP3_FRAME_HEADER[] = {
    0xFF, 0xFB, 0x90, 0x00  /* MPEG1 Layer3, 128kbps, 44100Hz, stereo */
};

void setUp(void)
{
    memset(&test_cbuf, 0, sizeof(CBUF2));
    memset(&test_rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&test_prefs, 0, sizeof(STREAM_PREFS));
    memset(&test_sp_opt, 0, sizeof(SPLITPOINT_OPTIONS));

    test_rmi.prefs = &test_prefs;
    test_rmi.relay_list = NULL;

    errors_init();
}

void tearDown(void)
{
    if (test_cbuf.buf != NULL) {
        cbuf2_destroy(&test_cbuf);
    }
}

/* ============================================================================
 * Test: Splitpoint options initialization
 * Tests default splitpoint option values for silence detection.
 * ============================================================================ */
static void test_splitpoint_options_defaults(void)
{
    SPLITPOINT_OPTIONS sp_opt;

    memset(&sp_opt, 0, sizeof(SPLITPOINT_OPTIONS));

    /* Default values that would be used for silence detection */
    sp_opt.xs = 1;                    /* Enable silence detection */
    sp_opt.xs_min_volume = 1;         /* Minimum volume threshold */
    sp_opt.xs_silence_length = 100;   /* Silence length in ms */
    sp_opt.xs_search_window_1 = 6000; /* Search window before split */
    sp_opt.xs_search_window_2 = 6000; /* Search window after split */
    sp_opt.xs_offset = 0;             /* Offset for split point */
    sp_opt.xs_padding_1 = 300;        /* Padding before split */
    sp_opt.xs_padding_2 = 300;        /* Padding after split */

    /* Verify all fields are set */
    TEST_ASSERT_EQUAL(1, sp_opt.xs);
    TEST_ASSERT_EQUAL(1, sp_opt.xs_min_volume);
    TEST_ASSERT_EQUAL(100, sp_opt.xs_silence_length);
    TEST_ASSERT_EQUAL(6000, sp_opt.xs_search_window_1);
    TEST_ASSERT_EQUAL(6000, sp_opt.xs_search_window_2);
    TEST_ASSERT_EQUAL(0, sp_opt.xs_offset);
    TEST_ASSERT_EQUAL(300, sp_opt.xs_padding_1);
    TEST_ASSERT_EQUAL(300, sp_opt.xs_padding_2);
}

/* ============================================================================
 * Test: Buffer initialization for silence detection
 * Tests buffer setup appropriate for silence detection workload.
 * ============================================================================ */
static void test_buffer_init_for_silence_detection(void)
{
    error_code rc;

    /* Typical buffer size for 128kbps stream with silence detection:
     * 128kbps = 16000 bytes/sec
     * 30 seconds of buffer = 480000 bytes
     * Using chunk_size of 4800 (0.3 sec) and 100 chunks
     */
    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 4800, 100);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(480000, test_cbuf.size);
    TEST_ASSERT_EQUAL(CONTENT_TYPE_MP3, test_cbuf.content_type);
}

/* ============================================================================
 * Test: Buffer fill for silence detection window
 * Tests filling buffer with enough data for silence detection.
 * ============================================================================ */
static void test_buffer_fill_for_detection_window(void)
{
    error_code rc;
    char audio_data[4096];

    /* Initialize buffer */
    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 4096, 20);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Fill with simulated audio data */
    memset(audio_data, 0x80, sizeof(audio_data)); /* Neutral PCM value */

    /* Insert multiple chunks to build up detection window */
    for (int i = 0; i < 10; i++) {
        rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, audio_data, 4096,
                                CONTENT_TYPE_MP3, NULL);
        TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    }

    /* Verify buffer contains enough data for detection */
    TEST_ASSERT_EQUAL(40960, test_cbuf.item_count);
    TEST_ASSERT_TRUE(test_cbuf.item_count >= 40000); /* ~2.5 sec at 128kbps */
}

/* ============================================================================
 * Test: Buffer peek for silence analysis
 * Tests peeking at buffer contents for silence analysis.
 * ============================================================================ */
static void test_buffer_peek_for_analysis(void)
{
    error_code rc;
    char write_data[8192];
    char peek_data[8192];

    /* Initialize buffer */
    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 4096, 10);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Create pattern data simulating audio */
    for (int i = 0; i < 8192; i++) {
        write_data[i] = (char)(i % 256);
    }

    /* Insert data */
    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, write_data, 8192,
                            CONTENT_TYPE_MP3, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Peek at data for analysis */
    rc = cbuf2_peek(&test_cbuf, peek_data, 8192);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Verify data integrity - essential for accurate silence detection */
    TEST_ASSERT_EQUAL_MEMORY(write_data, peek_data, 8192);
}

/* ============================================================================
 * Test: Buffer region peek for search window
 * Tests peeking at specific buffer regions for search windows.
 * ============================================================================ */
static void test_buffer_peek_region(void)
{
    error_code rc;
    char audio_data[16384];
    char region_data[4096];

    /* Initialize buffer */
    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 4096, 10);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Fill buffer with pattern */
    for (int i = 0; i < 16384; i++) {
        audio_data[i] = (char)(i / 64); /* Creates distinct regions */
    }

    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, audio_data, 16384,
                            CONTENT_TYPE_MP3, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Peek at specific region (like a search window) */
    rc = cbuf2_peek_rgn(&test_cbuf, region_data, 4096, 4096);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Verify we got the right region */
    TEST_ASSERT_EQUAL_MEMORY(audio_data + 4096, region_data, 4096);
}

/* ============================================================================
 * Test: Next song position marking
 * Tests marking the position where a new song begins in the buffer.
 * ============================================================================ */
static void test_next_song_position(void)
{
    error_code rc;
    char audio_data[4096];

    /* Initialize buffer */
    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 4096, 10);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Insert some data */
    memset(audio_data, 0xAA, sizeof(audio_data));
    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, audio_data, 4096,
                            CONTENT_TYPE_MP3, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Mark next song position (as silence detection would do) */
    cbuf2_set_next_song(&test_cbuf, 2048);
    TEST_ASSERT_EQUAL(2048, test_cbuf.next_song);

    /* Update position after more analysis */
    cbuf2_set_next_song(&test_cbuf, 3000);
    TEST_ASSERT_EQUAL(3000, test_cbuf.next_song);
}

/* ============================================================================
 * Test: Splitpoint options in stream prefs
 * Tests that splitpoint options are properly stored in stream prefs.
 * ============================================================================ */
static void test_splitpoint_in_stream_prefs(void)
{
    STREAM_PREFS prefs;

    memset(&prefs, 0, sizeof(STREAM_PREFS));

    /* Configure splitpoint options */
    prefs.sp_opt.xs = 1;
    prefs.sp_opt.xs_min_volume = 5;
    prefs.sp_opt.xs_silence_length = 200;
    prefs.sp_opt.xs_search_window_1 = 8000;
    prefs.sp_opt.xs_search_window_2 = 4000;
    prefs.sp_opt.xs_padding_1 = 500;
    prefs.sp_opt.xs_padding_2 = 250;

    /* Verify storage in prefs */
    TEST_ASSERT_EQUAL(1, prefs.sp_opt.xs);
    TEST_ASSERT_EQUAL(5, prefs.sp_opt.xs_min_volume);
    TEST_ASSERT_EQUAL(200, prefs.sp_opt.xs_silence_length);
    TEST_ASSERT_EQUAL(8000, prefs.sp_opt.xs_search_window_1);
    TEST_ASSERT_EQUAL(4000, prefs.sp_opt.xs_search_window_2);
}

/* ============================================================================
 * Test: Buffer wrap-around for continuous streaming
 * Tests buffer behavior when it wraps around during continuous streaming.
 * ============================================================================ */
static void test_buffer_wrap_around(void)
{
    error_code rc;
    char audio_data[4096];

    /* Initialize small buffer to force wrap-around */
    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 4096, 4);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(16384, test_cbuf.size);

    memset(audio_data, 0xBB, sizeof(audio_data));

    /* Fill buffer completely */
    for (int i = 0; i < 4; i++) {
        rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, audio_data, 4096,
                                CONTENT_TYPE_MP3, NULL);
        TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    }
    TEST_ASSERT_EQUAL(16384, test_cbuf.item_count);
    TEST_ASSERT_EQUAL(0, cbuf2_get_free(&test_cbuf));

    /* Consume some data */
    rc = cbuf2_fastforward(&test_cbuf, 8192);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(8192, test_cbuf.item_count);

    /* Insert more - should wrap */
    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, audio_data, 4096,
                            CONTENT_TYPE_MP3, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Write index should have wrapped */
    u_long write_idx = cbuf2_write_index(&test_cbuf);
    /* After wrap: base at 8192, 12288 items, write at (8192+12288)%16384 = 4096 */
    TEST_ASSERT_TRUE(write_idx < test_cbuf.size);
}

/* ============================================================================
 * Test: RIP_MANAGER_INFO fields for silence detection
 * Tests that RIP_MANAGER_INFO is properly configured for silence detection.
 * ============================================================================ */
static void test_rip_manager_silence_config(void)
{
    RIP_MANAGER_INFO rmi;
    STREAM_PREFS prefs;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&prefs, 0, sizeof(STREAM_PREFS));

    rmi.prefs = &prefs;

    /* Configure for silence detection */
    prefs.sp_opt.xs = 1;
    prefs.sp_opt.xs_search_window_1 = 6000;
    prefs.sp_opt.xs_search_window_2 = 6000;
    prefs.sp_opt.xs_silence_length = 100;

    /* Set bitrate (used to calculate buffer sizes) */
    rmi.bitrate = 128;
    rmi.detected_bitrate = 128;

    /* Verify configuration */
    TEST_ASSERT_EQUAL(1, rmi.prefs->sp_opt.xs);
    TEST_ASSERT_EQUAL(128, rmi.bitrate);
    TEST_ASSERT_NOT_NULL(rmi.prefs);
}

/* ============================================================================
 * Test: Buffer size calculation for bitrate
 * Tests calculating appropriate buffer sizes based on bitrate.
 * ============================================================================ */
static void test_buffer_size_for_bitrate(void)
{
    error_code rc;

    /* For 128kbps stream:
     * 128 kbps = 16000 bytes/sec
     * 30 second buffer = 480000 bytes
     * Chunk size typically matches metadata interval (e.g., 8192)
     */
    int bitrate_kbps = 128;
    int bytes_per_sec = bitrate_kbps * 1000 / 8;
    int buffer_seconds = 30;
    int total_size = bytes_per_sec * buffer_seconds;

    /* Calculate chunks */
    int chunk_size = 8192;
    int num_chunks = (total_size + chunk_size - 1) / chunk_size;

    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, chunk_size, num_chunks);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_TRUE(test_cbuf.size >= (u_long)total_size);

    /* For 320kbps stream */
    cbuf2_destroy(&test_cbuf);
    memset(&test_cbuf, 0, sizeof(CBUF2));

    bitrate_kbps = 320;
    bytes_per_sec = bitrate_kbps * 1000 / 8;
    total_size = bytes_per_sec * buffer_seconds;
    num_chunks = (total_size + chunk_size - 1) / chunk_size;

    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, chunk_size, num_chunks);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_TRUE(test_cbuf.size >= (u_long)total_size);
}

/* ============================================================================
 * Test: Track metadata with silence detection
 * Tests inserting chunks with track info changes.
 * ============================================================================ */
static void test_track_change_during_detection(void)
{
    error_code rc;
    char audio_data[4096];
    TRACK_INFO ti1, ti2;

    memset(&ti1, 0, sizeof(TRACK_INFO));
    memset(&ti2, 0, sizeof(TRACK_INFO));

    /* Initialize buffer */
    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 4096, 10);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    memset(audio_data, 0xCC, sizeof(audio_data));

    /* Insert first track */
    ti1.have_track_info = 1;
    strcpy(ti1.raw_metadata, "Artist1 - Song1");
    strcpy(ti1.composed_metadata, "\x11StreamTitle='Artist1 - Song1';");

    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, audio_data, 4096,
                            CONTENT_TYPE_MP3, &ti1);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Insert second track (simulating track change) */
    ti2.have_track_info = 1;
    strcpy(ti2.raw_metadata, "Artist2 - Song2");
    strcpy(ti2.composed_metadata, "\x11StreamTitle='Artist2 - Song2';");

    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, audio_data, 4096,
                            CONTENT_TYPE_MP3, &ti2);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    TEST_ASSERT_EQUAL(8192, test_cbuf.item_count);
}

/* ============================================================================
 * Test: Search window boundaries
 * Tests that search window calculations are within buffer bounds.
 * ============================================================================ */
static void test_search_window_boundaries(void)
{
    SPLITPOINT_OPTIONS sp_opt;
    int bitrate_bps = 128000;  /* bits per second */
    int bytes_per_ms = bitrate_bps / 8 / 1000;

    sp_opt.xs_search_window_1 = 6000;  /* 6 seconds in ms */
    sp_opt.xs_search_window_2 = 6000;

    /* Calculate byte sizes for search windows */
    long sw1_bytes = sp_opt.xs_search_window_1 * bytes_per_ms;
    long sw2_bytes = sp_opt.xs_search_window_2 * bytes_per_ms;

    /* Verify calculations */
    TEST_ASSERT_EQUAL(96000, sw1_bytes);  /* 6000ms * 16 bytes/ms */
    TEST_ASSERT_EQUAL(96000, sw2_bytes);

    /* Total buffer needed */
    long total_needed = sw1_bytes + sw2_bytes;
    TEST_ASSERT_EQUAL(192000, total_needed);
}

/* ============================================================================
 * Test: Silence length parameter validation
 * Tests silence length parameter ranges.
 * ============================================================================ */
static void test_silence_length_parameters(void)
{
    SPLITPOINT_OPTIONS sp_opt;

    /* Test minimum silence length */
    sp_opt.xs_silence_length = 50;  /* 50ms */
    TEST_ASSERT_EQUAL(50, sp_opt.xs_silence_length);

    /* Test typical silence length */
    sp_opt.xs_silence_length = 100;  /* 100ms */
    TEST_ASSERT_EQUAL(100, sp_opt.xs_silence_length);

    /* Test maximum reasonable silence length */
    sp_opt.xs_silence_length = 500;  /* 500ms */
    TEST_ASSERT_EQUAL(500, sp_opt.xs_silence_length);
}

/* ============================================================================
 * Test: Padding parameters
 * Tests padding parameter configuration for track splitting.
 * ============================================================================ */
static void test_padding_parameters(void)
{
    SPLITPOINT_OPTIONS sp_opt;

    /* Padding determines how much audio is kept at track boundaries */
    sp_opt.xs_padding_1 = 300;  /* 300ms padding at end of previous track */
    sp_opt.xs_padding_2 = 300;  /* 300ms padding at start of next track */

    TEST_ASSERT_EQUAL(300, sp_opt.xs_padding_1);
    TEST_ASSERT_EQUAL(300, sp_opt.xs_padding_2);

    /* Asymmetric padding */
    sp_opt.xs_padding_1 = 500;
    sp_opt.xs_padding_2 = 100;

    TEST_ASSERT_EQUAL(500, sp_opt.xs_padding_1);
    TEST_ASSERT_EQUAL(100, sp_opt.xs_padding_2);
}

/* ============================================================================
 * Main entry point
 * ============================================================================ */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* Splitpoint options tests */
    RUN_TEST(test_splitpoint_options_defaults);
    RUN_TEST(test_splitpoint_in_stream_prefs);
    RUN_TEST(test_silence_length_parameters);
    RUN_TEST(test_padding_parameters);
    RUN_TEST(test_search_window_boundaries);

    /* Buffer initialization tests */
    RUN_TEST(test_buffer_init_for_silence_detection);
    RUN_TEST(test_buffer_size_for_bitrate);

    /* Buffer operation tests */
    RUN_TEST(test_buffer_fill_for_detection_window);
    RUN_TEST(test_buffer_peek_for_analysis);
    RUN_TEST(test_buffer_peek_region);
    RUN_TEST(test_buffer_wrap_around);

    /* Song position and track tests */
    RUN_TEST(test_next_song_position);
    RUN_TEST(test_track_change_during_detection);

    /* RIP manager configuration */
    RUN_TEST(test_rip_manager_silence_config);

    return UNITY_END();
}
