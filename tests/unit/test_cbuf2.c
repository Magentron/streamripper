/* test_cbuf2.c - Unit tests for lib/cbuf2.c
 *
 * Tests for circular buffer implementation.
 * The circular buffer is used for audio streaming data.
 *
 * Note: We provide stub implementations for threadlib semaphore functions
 * to avoid dependencies on the full threadlib implementation.
 */
#include <string.h>
#include <stdlib.h>
#include <unity.h>

#include "cbuf2.h"
#include "errors.h"
#include "srtypes.h"

/* Stub implementations for threadlib semaphore functions
 * These are needed because cbuf2.c uses them but we don't want to
 * link with the full threadlib implementation */
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

/* Stub for debug_printf - cbuf2.c uses this */
void debug_printf(char *fmt, ...)
{
    (void)fmt;
}

/* Stub for relaylib_disconnect - cbuf2.c uses this */
void relaylib_disconnect(RIP_MANAGER_INFO *rmi, RELAY_LIST *prev, RELAY_LIST *ptr)
{
    (void)rmi;
    (void)prev;
    (void)ptr;
}

/* Stub for rip_ogg_process_chunk - cbuf2.c uses this for OGG content */
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

/* Test fixture data */
static CBUF2 test_cbuf;
static RIP_MANAGER_INFO test_rmi;
static TRACK_INFO test_ti;

void setUp(void)
{
    memset(&test_cbuf, 0, sizeof(CBUF2));
    memset(&test_rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&test_ti, 0, sizeof(TRACK_INFO));

    /* Initialize relay list pointer to NULL */
    test_rmi.relay_list = NULL;
}

void tearDown(void)
{
    /* Clean up any allocated buffer */
    if (test_cbuf.buf != NULL) {
        free(test_cbuf.buf);
        test_cbuf.buf = NULL;
    }
}

/* ============================================================================
 * cbuf2_init tests
 * ============================================================================ */

/* Test: cbuf2_init with valid parameters succeeds */
static void test_cbuf2_init_valid_params(void)
{
    error_code rc;

    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(CONTENT_TYPE_MP3, test_cbuf.content_type);
    TEST_ASSERT_EQUAL(0, test_cbuf.have_relay);
    TEST_ASSERT_EQUAL(1024, test_cbuf.chunk_size);
    TEST_ASSERT_EQUAL(10, test_cbuf.num_chunks);
    TEST_ASSERT_EQUAL(10240, test_cbuf.size);  /* chunk_size * num_chunks */
    TEST_ASSERT_NOT_NULL(test_cbuf.buf);
    TEST_ASSERT_EQUAL(0, test_cbuf.base_idx);
    TEST_ASSERT_EQUAL(0, test_cbuf.item_count);
    TEST_ASSERT_EQUAL(0, test_cbuf.next_song);
}

/* Test: cbuf2_init with zero chunk_size returns error */
static void test_cbuf2_init_zero_chunk_size(void)
{
    error_code rc;

    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 0, 10);

    TEST_ASSERT_EQUAL(SR_ERROR_INVALID_PARAM, rc);
    TEST_ASSERT_NULL(test_cbuf.buf);
}

/* Test: cbuf2_init with zero num_chunks returns error */
static void test_cbuf2_init_zero_num_chunks(void)
{
    error_code rc;

    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 0);

    TEST_ASSERT_EQUAL(SR_ERROR_INVALID_PARAM, rc);
    TEST_ASSERT_NULL(test_cbuf.buf);
}

/* Test: cbuf2_init with relay enabled */
static void test_cbuf2_init_with_relay(void)
{
    error_code rc;

    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 512, 5);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1, test_cbuf.have_relay);
    TEST_ASSERT_EQUAL(512, test_cbuf.chunk_size);
    TEST_ASSERT_EQUAL(5, test_cbuf.num_chunks);
    TEST_ASSERT_EQUAL(2560, test_cbuf.size);
}

/* Test: cbuf2_init for OGG content type */
static void test_cbuf2_init_ogg_content(void)
{
    error_code rc;

    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 0, 4096, 8);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(CONTENT_TYPE_OGG, test_cbuf.content_type);
    TEST_ASSERT_EQUAL(0, test_cbuf.song_page);
    TEST_ASSERT_EQUAL(0, test_cbuf.song_page_done);
}

/* ============================================================================
 * cbuf2_get_free tests
 * ============================================================================ */

/* Test: cbuf2_get_free returns full size for empty buffer */
static void test_cbuf2_get_free_empty_buffer(void)
{
    u_long free_space;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    free_space = cbuf2_get_free(&test_cbuf);

    TEST_ASSERT_EQUAL(10240, free_space);
}

/* Test: cbuf2_get_free returns zero for full buffer */
static void test_cbuf2_get_free_full_buffer(void)
{
    u_long free_space;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Manually fill the buffer */
    test_cbuf.item_count = test_cbuf.size;

    free_space = cbuf2_get_free(&test_cbuf);

    TEST_ASSERT_EQUAL(0, free_space);
}

/* Test: cbuf2_get_free returns partial space */
static void test_cbuf2_get_free_partial_buffer(void)
{
    u_long free_space;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Partially fill the buffer */
    test_cbuf.item_count = 5120;  /* Half full */

    free_space = cbuf2_get_free(&test_cbuf);

    TEST_ASSERT_EQUAL(5120, free_space);
}

/* ============================================================================
 * cbuf2_write_index tests
 * ============================================================================ */

/* Test: cbuf2_write_index at start of buffer */
static void test_cbuf2_write_index_empty(void)
{
    u_long write_idx;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    write_idx = cbuf2_write_index(&test_cbuf);

    TEST_ASSERT_EQUAL(0, write_idx);
}

/* Test: cbuf2_write_index with data in buffer */
static void test_cbuf2_write_index_with_data(void)
{
    u_long write_idx;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Add some data */
    test_cbuf.item_count = 2048;

    write_idx = cbuf2_write_index(&test_cbuf);

    TEST_ASSERT_EQUAL(2048, write_idx);
}

/* Test: cbuf2_write_index wraps around */
static void test_cbuf2_write_index_wrap_around(void)
{
    u_long write_idx;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Set base near end and add data that wraps */
    test_cbuf.base_idx = 9000;
    test_cbuf.item_count = 2000;
    /* write_idx = (9000 + 2000) % 10240 = 11000 - 10240 = 760 */

    write_idx = cbuf2_write_index(&test_cbuf);

    TEST_ASSERT_EQUAL(760, write_idx);
}

/* ============================================================================
 * cbuf2_get_free_tail tests
 * ============================================================================ */

/* Test: cbuf2_get_free_tail at start of empty buffer */
static void test_cbuf2_get_free_tail_empty(void)
{
    u_long free_tail;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    free_tail = cbuf2_get_free_tail(&test_cbuf);

    /* Empty buffer, write at 0, so free tail = size */
    TEST_ASSERT_EQUAL(10240, free_tail);
}

/* Test: cbuf2_get_free_tail near end of buffer */
static void test_cbuf2_get_free_tail_near_end(void)
{
    u_long free_tail;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Write index at 9000, so free_tail to end = 10240 - 9000 = 1240 */
    test_cbuf.item_count = 9000;

    free_tail = cbuf2_get_free_tail(&test_cbuf);

    TEST_ASSERT_EQUAL(1240, free_tail);
}

/* ============================================================================
 * cbuf2_set_next_song tests
 * ============================================================================ */

/* Test: cbuf2_set_next_song sets position correctly */
static void test_cbuf2_set_next_song(void)
{
    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    cbuf2_set_next_song(&test_cbuf, 5000);

    TEST_ASSERT_EQUAL(5000, test_cbuf.next_song);
}

/* Test: cbuf2_set_next_song can be called multiple times */
static void test_cbuf2_set_next_song_update(void)
{
    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    cbuf2_set_next_song(&test_cbuf, 1000);
    TEST_ASSERT_EQUAL(1000, test_cbuf.next_song);

    cbuf2_set_next_song(&test_cbuf, 3000);
    TEST_ASSERT_EQUAL(3000, test_cbuf.next_song);
}

/* ============================================================================
 * cbuf2_peek tests
 * ============================================================================ */

/* Test: cbuf2_peek returns error on empty buffer */
static void test_cbuf2_peek_empty_buffer(void)
{
    char data[100];
    error_code rc;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    rc = cbuf2_peek(&test_cbuf, data, 100);

    TEST_ASSERT_EQUAL(SR_ERROR_BUFFER_EMPTY, rc);
}

/* Test: cbuf2_peek returns error when requesting more than available */
static void test_cbuf2_peek_request_too_much(void)
{
    char data[2000];
    error_code rc;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Add some data */
    test_cbuf.item_count = 1000;
    memset(test_cbuf.buf, 'A', 1000);

    /* Request more than available - returns early before locking */
    rc = cbuf2_peek(&test_cbuf, data, 2000);

    TEST_ASSERT_EQUAL(SR_ERROR_BUFFER_EMPTY, rc);
}

/* Test: cbuf2_peek successfully reads data */
static void test_cbuf2_peek_success(void)
{
    char data[500];
    error_code rc;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Add test data */
    test_cbuf.item_count = 1000;
    memset(test_cbuf.buf, 'X', 1000);

    rc = cbuf2_peek(&test_cbuf, data, 500);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL('X', data[0]);
    TEST_ASSERT_EQUAL('X', data[499]);

    /* Verify item_count unchanged (peek doesn't remove data) */
    TEST_ASSERT_EQUAL(1000, test_cbuf.item_count);
}

/* ============================================================================
 * cbuf2_fastforward tests
 * ============================================================================ */

/* Test: cbuf2_fastforward returns error on empty buffer */
static void test_cbuf2_fastforward_empty_buffer(void)
{
    error_code rc;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    rc = cbuf2_fastforward(&test_cbuf, 100);

    TEST_ASSERT_EQUAL(SR_ERROR_BUFFER_EMPTY, rc);
}

/* Test: cbuf2_fastforward succeeds with enough data */
static void test_cbuf2_fastforward_success(void)
{
    error_code rc;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Add data */
    test_cbuf.item_count = 2000;
    test_cbuf.base_idx = 0;

    rc = cbuf2_fastforward(&test_cbuf, 500);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1500, test_cbuf.item_count);
    TEST_ASSERT_EQUAL(500, test_cbuf.base_idx);
}

/* Test: cbuf2_fastforward wraps base_idx */
static void test_cbuf2_fastforward_wrap(void)
{
    error_code rc;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Set up buffer near end */
    test_cbuf.item_count = 2000;
    test_cbuf.base_idx = 9500;

    rc = cbuf2_fastforward(&test_cbuf, 1000);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1000, test_cbuf.item_count);
    /* base_idx = (9500 + 1000) - 10240 = 260 */
    TEST_ASSERT_EQUAL(260, test_cbuf.base_idx);
}

/* ============================================================================
 * cbuf2_destroy tests
 * ============================================================================ */

/* Test: cbuf2_destroy cleans up properly */
static void test_cbuf2_destroy_cleans_up(void)
{
    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    TEST_ASSERT_NOT_NULL(test_cbuf.buf);

    cbuf2_destroy(&test_cbuf);

    TEST_ASSERT_NULL(test_cbuf.buf);
}

/* Test: cbuf2_destroy handles NULL buffer gracefully */
static void test_cbuf2_destroy_null_buffer(void)
{
    /* Create cbuf without initializing */
    memset(&test_cbuf, 0, sizeof(CBUF2));

    /* Should not crash */
    cbuf2_destroy(&test_cbuf);

    TEST_ASSERT_NULL(test_cbuf.buf);
}

/* ============================================================================
 * Buffer state tests (edge cases)
 * ============================================================================ */

/* Test: Empty buffer state */
static void test_cbuf2_empty_state(void)
{
    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    TEST_ASSERT_EQUAL(0, test_cbuf.item_count);
    TEST_ASSERT_EQUAL(0, test_cbuf.base_idx);
    TEST_ASSERT_EQUAL(10240, cbuf2_get_free(&test_cbuf));
    TEST_ASSERT_EQUAL(0, cbuf2_write_index(&test_cbuf));
}

/* Test: Full buffer state */
static void test_cbuf2_full_state(void)
{
    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Manually fill */
    test_cbuf.item_count = test_cbuf.size;

    TEST_ASSERT_EQUAL(test_cbuf.size, test_cbuf.item_count);
    TEST_ASSERT_EQUAL(0, cbuf2_get_free(&test_cbuf));
    TEST_ASSERT_EQUAL(test_cbuf.base_idx, cbuf2_write_index(&test_cbuf));
}

/* Test: Various content types */
static void test_cbuf2_aac_content_type(void)
{
    error_code rc;

    rc = cbuf2_init(&test_cbuf, CONTENT_TYPE_AAC, 0, 2048, 4);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(CONTENT_TYPE_AAC, test_cbuf.content_type);
}

/* ============================================================================
 * cbuf2_insert_chunk tests
 * ============================================================================ */

/* Test: cbuf2_insert_chunk inserts data correctly */
static void test_cbuf2_insert_chunk_basic(void)
{
    error_code rc;
    char data[1024];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Prepare test data */
    memset(data, 'A', sizeof(data));

    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, data, 1024, CONTENT_TYPE_MP3, NULL);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1024, test_cbuf.item_count);
}

/* Test: cbuf2_insert_chunk with track info */
static void test_cbuf2_insert_chunk_with_track_info(void)
{
    error_code rc;
    char data[1024];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Prepare test data */
    memset(data, 'B', sizeof(data));

    /* Set up track info */
    test_ti.have_track_info = 1;
    strcpy(test_ti.composed_metadata, "\x01Test");

    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, data, 1024, CONTENT_TYPE_MP3, &test_ti);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1024, test_cbuf.item_count);
}

/* Test: cbuf2_insert_chunk returns error when buffer full */
static void test_cbuf2_insert_chunk_buffer_full(void)
{
    error_code rc;
    char data[1024];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Fill buffer */
    test_cbuf.item_count = test_cbuf.size;

    memset(data, 'C', sizeof(data));

    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, data, 1024, CONTENT_TYPE_MP3, NULL);

    TEST_ASSERT_EQUAL(SR_ERROR_BUFFER_FULL, rc);
}

/* ============================================================================
 * cbuf2_peek_rgn tests
 * ============================================================================ */

/* Test: cbuf2_peek_rgn reads from specific region */
static void test_cbuf2_peek_rgn_basic(void)
{
    error_code rc;
    char out_buf[100];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Add data */
    test_cbuf.item_count = 1000;
    memset(test_cbuf.buf, 'X', 500);
    memset(test_cbuf.buf + 500, 'Y', 500);

    rc = cbuf2_peek_rgn(&test_cbuf, out_buf, 500, 100);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL('Y', out_buf[0]);
}

/* Test: cbuf2_peek_rgn returns error if length exceeds item_count */
static void test_cbuf2_peek_rgn_too_long(void)
{
    error_code rc;
    char out_buf[100];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Add minimal data */
    test_cbuf.item_count = 50;

    rc = cbuf2_peek_rgn(&test_cbuf, out_buf, 0, 100);

    TEST_ASSERT_EQUAL(SR_ERROR_BUFFER_EMPTY, rc);
}

/* Test: cbuf2_peek_rgn wraps around buffer */
static void test_cbuf2_peek_rgn_wrap_around(void)
{
    error_code rc;
    char out_buf[200];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Fill buffer near end to force wrap */
    test_cbuf.base_idx = 9900;
    test_cbuf.item_count = 500;

    /* Fill test data - first part at end, second part at beginning */
    memset(test_cbuf.buf + 9900, 'A', 340);  /* To end of buffer */
    memset(test_cbuf.buf, 'B', 160);         /* Wrapped portion */

    /* Read from offset that wraps around
     * start_offset = 100, so starts at 9900 + 100 = 10000
     * We read 200 bytes starting at index 10000
     * Bytes 10000-10239 (240 bytes) are 'A'
     * Then wraps to bytes 0-159 (160 bytes) which are 'B'
     * So out_buf[0..239] = 'A', out_buf[240..399] would be 'B' but we only read 200
     * Actually: sidx = (9900 + 100) % 10240 = 10000
     * First part: 10000 to 10239 = 240 bytes of 'A'
     * But we only read 200 bytes total, so all should be 'A'
     */
    rc = cbuf2_peek_rgn(&test_cbuf, out_buf, 100, 200);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL('A', out_buf[0]);    /* First part */
    TEST_ASSERT_EQUAL('A', out_buf[199]);  /* Still in first part, doesn't wrap */
}

/* ============================================================================
 * cbuf2_extract tests
 * ============================================================================ */

/* Test: cbuf2_extract basic extraction without relay */
static void test_cbuf2_extract_basic(void)
{
    error_code rc;
    char extract_buf[1024];
    u_long curr_song;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Add test data */
    test_cbuf.item_count = 2048;
    memset(test_cbuf.buf, 'T', 2048);

    rc = cbuf2_extract(&test_rmi, &test_cbuf, extract_buf, 1024, &curr_song);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1024, test_cbuf.item_count);  /* 2048 - 1024 */
    TEST_ASSERT_EQUAL(1024, test_cbuf.base_idx);
    TEST_ASSERT_EQUAL('T', extract_buf[0]);
    TEST_ASSERT_EQUAL('T', extract_buf[1023]);
}

/* Test: cbuf2_extract returns error when not enough data */
static void test_cbuf2_extract_insufficient_data(void)
{
    error_code rc;
    char extract_buf[1024];
    u_long curr_song;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Add less data than requested */
    test_cbuf.item_count = 500;

    rc = cbuf2_extract(&test_rmi, &test_cbuf, extract_buf, 1024, &curr_song);

    TEST_ASSERT_EQUAL(SR_ERROR_BUFFER_EMPTY, rc);
}

/* Test: cbuf2_extract with wrap-around */
static void test_cbuf2_extract_wrap_around(void)
{
    error_code rc;
    char extract_buf[500];
    u_long curr_song;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Position data near end to force wrap */
    test_cbuf.base_idx = 9900;
    test_cbuf.item_count = 500;
    memset(test_cbuf.buf + 9900, 'X', 340);  /* To end */
    memset(test_cbuf.buf, 'Y', 160);         /* Wrapped */

    rc = cbuf2_extract(&test_rmi, &test_cbuf, extract_buf, 500, &curr_song);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL('X', extract_buf[0]);
    TEST_ASSERT_EQUAL('Y', extract_buf[499]);
    TEST_ASSERT_EQUAL(0, test_cbuf.item_count);
    /* base_idx = (9900 + 500) % 10240 = 160 */
    TEST_ASSERT_EQUAL(160, test_cbuf.base_idx);
}

/* Test: cbuf2_extract updates next_song correctly */
static void test_cbuf2_extract_next_song_update(void)
{
    error_code rc;
    char extract_buf[1024];
    u_long curr_song;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Set next_song position */
    test_cbuf.next_song = 2000;
    test_cbuf.item_count = 2048;
    memset(test_cbuf.buf, 'S', 2048);

    rc = cbuf2_extract(&test_rmi, &test_cbuf, extract_buf, 1024, &curr_song);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(2000, curr_song);  /* Returns old value */
    TEST_ASSERT_EQUAL(976, test_cbuf.next_song);  /* 2000 - 1024 */
}

/* Test: cbuf2_extract resets next_song when it becomes less than count */
static void test_cbuf2_extract_next_song_reset(void)
{
    error_code rc;
    char extract_buf[1024];
    u_long curr_song;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Set next_song to less than what we'll extract */
    test_cbuf.next_song = 500;
    test_cbuf.item_count = 2048;
    memset(test_cbuf.buf, 'R', 2048);

    rc = cbuf2_extract(&test_rmi, &test_cbuf, extract_buf, 1024, &curr_song);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(500, curr_song);
    TEST_ASSERT_EQUAL(0, test_cbuf.next_song);  /* Reset to 0 */
}

/* ============================================================================
 * cbuf2_peek with wrap-around tests
 * ============================================================================ */

/* Test: cbuf2_peek with wrap-around buffer */
static void test_cbuf2_peek_wrap_around(void)
{
    error_code rc;
    char peek_buf[400];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Position data to wrap around */
    test_cbuf.base_idx = 10000;
    test_cbuf.item_count = 400;
    memset(test_cbuf.buf + 10000, 'P', 240);  /* To end */
    memset(test_cbuf.buf, 'Q', 160);          /* Wrapped */

    rc = cbuf2_peek(&test_cbuf, peek_buf, 400);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL('P', peek_buf[0]);
    TEST_ASSERT_EQUAL('Q', peek_buf[399]);
    /* Verify peek doesn't modify item_count or base_idx */
    TEST_ASSERT_EQUAL(400, test_cbuf.item_count);
    TEST_ASSERT_EQUAL(10000, test_cbuf.base_idx);
}

/* ============================================================================
 * cbuf2_insert_chunk edge case tests
 * ============================================================================ */

/* Test: cbuf2_insert_chunk multiple insertions */
static void test_cbuf2_insert_chunk_multiple(void)
{
    error_code rc;
    char data1[512];
    char data2[512];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    memset(data1, '1', sizeof(data1));
    memset(data2, '2', sizeof(data2));

    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, data1, 512, CONTENT_TYPE_MP3, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(512, test_cbuf.item_count);

    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, data2, 512, CONTENT_TYPE_MP3, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1024, test_cbuf.item_count);

    /* Verify data integrity */
    TEST_ASSERT_EQUAL('1', test_cbuf.buf[0]);
    TEST_ASSERT_EQUAL('2', test_cbuf.buf[512]);
}

/* Test: cbuf2_insert_chunk with wrap-around */
static void test_cbuf2_insert_chunk_wrap_around(void)
{
    error_code rc;
    char data[500];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Position write pointer near end by setting base_idx and item_count */
    test_cbuf.base_idx = 1000;  /* Start somewhere in middle */
    test_cbuf.item_count = 9100; /* write_idx will be at (1000 + 9100) % 10240 = 10100 - 10240 = -140 = 9860 */
    /* Actually: write_idx = cbuf2_add(1000, 9100) = 10100 % 10240 = 9860 */
    /* Hmm, that doesn't wrap. Let me recalculate:
     * If base_idx = 9900, item_count = 0, write_idx = 9900
     * To make write_idx wrap, we need item_count such that base + count >= 10240
     * base = 9900, count = 400 => write_idx = 10300 % 10240 = 60 */

    test_cbuf.base_idx = 9900;
    test_cbuf.item_count = 0;  /* Empty buffer starting at 9900 */

    memset(data, 'W', sizeof(data));

    /* Write 500 bytes starting at index 9900
     * This writes: [9900..10239] = 340 bytes, then [0..159] = 160 bytes
     * Total = 500 bytes */
    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, data, 500, CONTENT_TYPE_MP3, NULL);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(500, test_cbuf.item_count);
    /* Verify data wrapped */
    TEST_ASSERT_EQUAL('W', test_cbuf.buf[9900]);  /* First part */
    TEST_ASSERT_EQUAL('W', test_cbuf.buf[10239]); /* Last byte before wrap */
    TEST_ASSERT_EQUAL('W', test_cbuf.buf[0]);     /* Wrapped portion start */
    TEST_ASSERT_EQUAL('W', test_cbuf.buf[159]);   /* Wrapped portion end */
}

/* Test: cbuf2_insert_chunk with OGG content type */
static void test_cbuf2_insert_chunk_ogg_content(void)
{
    error_code rc;
    char data[1024];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 0, 1024, 10);

    memset(data, 'O', sizeof(data));

    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, data, 1024, CONTENT_TYPE_OGG, NULL);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1024, test_cbuf.item_count);
}

/* ============================================================================
 * cbuf2_get_used/get_free edge cases
 * ============================================================================ */

/* Test: cbuf2_get_free with various fill levels */
static void test_cbuf2_get_free_quarter_full(void)
{
    u_long free_space;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* 25% full */
    test_cbuf.item_count = 2560;

    free_space = cbuf2_get_free(&test_cbuf);

    TEST_ASSERT_EQUAL(7680, free_space);  /* 10240 - 2560 */
}

/* Test: cbuf2_get_free_tail when write wraps to beginning */
static void test_cbuf2_get_free_tail_after_wrap(void)
{
    u_long free_tail;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Buffer has wrapped: base at 5000, 8000 bytes used */
    test_cbuf.base_idx = 5000;
    test_cbuf.item_count = 8000;
    /* write_idx = (5000 + 8000) % 10240 = 2760 */
    /* free = 10240 - 8000 = 2240 */
    /* free_tail = 10240 - 2760 = 7480, but free = 2240, so return 2240 */

    free_tail = cbuf2_get_free_tail(&test_cbuf);

    TEST_ASSERT_EQUAL(2240, free_tail);
}

/* ============================================================================
 * cbuf2_advance_ogg tests
 * ============================================================================ */

/* Test: cbuf2_advance_ogg when buffer has enough space */
static void test_cbuf2_advance_ogg_sufficient_space(void)
{
    error_code rc;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 0, 1024, 10);

    /* Buffer has 5000 bytes used, request 1000 bytes free */
    test_cbuf.item_count = 5000;

    rc = cbuf2_advance_ogg(&test_rmi, &test_cbuf, 1000);

    /* Already have 5240 bytes free (10240 - 5000), so no advancement needed */
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(5000, test_cbuf.item_count);  /* Unchanged */
}

/* Test: cbuf2_advance_ogg with empty page list */
static void test_cbuf2_advance_ogg_empty_page_list(void)
{
    error_code rc;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 0, 1024, 10);

    /* Fill buffer more than requested */
    test_cbuf.item_count = 9500;

    rc = cbuf2_advance_ogg(&test_rmi, &test_cbuf, 1000);

    /* Should advance even with empty page list */
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
}

/* ============================================================================
 * cbuf2_init_relay_entry tests
 * ============================================================================ */

/* Test: cbuf2_init_relay_entry for MP3 without metadata */
static void test_cbuf2_init_relay_entry_mp3_no_metadata(void)
{
    error_code rc;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 0;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 1024, 10);
    test_cbuf.item_count = 5000;

    rc = cbuf2_init_relay_entry(&test_cbuf, &relay, 2000);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    /* offset = 5000 - 2000 = 3000
     * aligned to chunk boundary = (3000 / 1024) * 1024 = 2 * 1024 = 2048 */
    TEST_ASSERT_EQUAL(2048, relay.m_cbuf_offset);
    TEST_ASSERT_EQUAL(1024, relay.m_buffer_size);  /* chunk_size */
    TEST_ASSERT_NOT_NULL(relay.m_buffer);

    /* Clean up */
    free(relay.m_buffer);
}

/* Test: cbuf2_init_relay_entry for MP3 with ICY metadata */
static void test_cbuf2_init_relay_entry_mp3_with_metadata(void)
{
    error_code rc;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 1;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 1024, 10);
    test_cbuf.item_count = 8000;

    rc = cbuf2_init_relay_entry(&test_cbuf, &relay, 3000);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    /* offset = 8000 - 3000 = 5000
     * aligned to chunk boundary = (5000 / 1024) * 1024 = 4 * 1024 = 4096 */
    TEST_ASSERT_EQUAL(4096, relay.m_cbuf_offset);
    /* Buffer size = chunk_size + max metadata (16 * 256) */
    TEST_ASSERT_EQUAL(5120, relay.m_buffer_size);  /* 1024 + 4096 */
    TEST_ASSERT_NOT_NULL(relay.m_buffer);

    /* Clean up */
    free(relay.m_buffer);
}

/* Test: cbuf2_init_relay_entry when burst_request exceeds item_count */
static void test_cbuf2_init_relay_entry_burst_exceeds_count(void)
{
    error_code rc;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 0;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 1024, 10);
    test_cbuf.item_count = 1000;

    /* Request more than available */
    rc = cbuf2_init_relay_entry(&test_cbuf, &relay, 5000);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(0, relay.m_cbuf_offset);  /* Clamped to 0 */
    TEST_ASSERT_NOT_NULL(relay.m_buffer);

    /* Clean up */
    free(relay.m_buffer);
}

/* Test: cbuf2_init_relay_entry aligns to chunk boundary for MP3 */
static void test_cbuf2_init_relay_entry_chunk_alignment(void)
{
    error_code rc;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 0;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 1024, 10);
    test_cbuf.item_count = 6000;

    /* Request 2500 bytes, which doesn't align to chunk boundary */
    rc = cbuf2_init_relay_entry(&test_cbuf, &relay, 2500);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    /* offset = 6000 - 2500 = 3500 */
    /* aligned = (3500 / 1024) * 1024 = 3 * 1024 = 3072 */
    TEST_ASSERT_EQUAL(3072, relay.m_cbuf_offset);

    /* Clean up */
    free(relay.m_buffer);
}

/* ============================================================================
 * cbuf2_ogg_peek_song tests
 * ============================================================================ */

/* Test: cbuf2_ogg_peek_song with empty page list */
static void test_cbuf2_ogg_peek_song_empty_list(void)
{
    error_code rc;
    char out_buf[1024];
    unsigned long amt_filled;
    int eos;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 0, 1024, 10);

    rc = cbuf2_ogg_peek_song(&test_cbuf, out_buf, 1024, &amt_filled, &eos);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(0, amt_filled);
    TEST_ASSERT_EQUAL(0, eos);
}

/* ============================================================================
 * Edge case and stress tests
 * ============================================================================ */

/* Test: Multiple operations on same buffer */
static void test_cbuf2_multiple_operations_sequence(void)
{
    error_code rc;
    char data[512];
    char peek_buf[512];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Insert */
    memset(data, 'M', sizeof(data));
    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, data, 512, CONTENT_TYPE_MP3, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Peek */
    rc = cbuf2_peek(&test_cbuf, peek_buf, 256);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL('M', peek_buf[0]);

    /* Fast forward */
    rc = cbuf2_fastforward(&test_cbuf, 128);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(384, test_cbuf.item_count);

    /* Peek again */
    rc = cbuf2_peek(&test_cbuf, peek_buf, 128);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
}

/* Test: cbuf2 with very small buffer */
static void test_cbuf2_small_buffer(void)
{
    error_code rc;
    CBUF2 small_cbuf;

    memset(&small_cbuf, 0, sizeof(CBUF2));

    rc = cbuf2_init(&small_cbuf, CONTENT_TYPE_MP3, 0, 64, 2);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(128, small_cbuf.size);
    TEST_ASSERT_NOT_NULL(small_cbuf.buf);

    cbuf2_destroy(&small_cbuf);
}

/* Test: cbuf2 with large buffer */
static void test_cbuf2_large_buffer(void)
{
    error_code rc;
    CBUF2 large_cbuf;

    memset(&large_cbuf, 0, sizeof(CBUF2));

    rc = cbuf2_init(&large_cbuf, CONTENT_TYPE_MP3, 0, 8192, 64);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(524288, large_cbuf.size);  /* 8192 * 64 */
    TEST_ASSERT_NOT_NULL(large_cbuf.buf);

    cbuf2_destroy(&large_cbuf);
}

/* Test: cbuf2_fastforward exactly to empty */
static void test_cbuf2_fastforward_to_empty(void)
{
    error_code rc;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    test_cbuf.item_count = 1500;
    test_cbuf.base_idx = 0;

    rc = cbuf2_fastforward(&test_cbuf, 1500);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(0, test_cbuf.item_count);
    TEST_ASSERT_EQUAL(1500, test_cbuf.base_idx);
}

/* Test: cbuf2_fastforward requesting too much */
static void test_cbuf2_fastforward_too_much(void)
{
    error_code rc;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    test_cbuf.item_count = 500;

    rc = cbuf2_fastforward(&test_cbuf, 1000);

    TEST_ASSERT_EQUAL(SR_ERROR_BUFFER_EMPTY, rc);
    /* Verify nothing changed */
    TEST_ASSERT_EQUAL(500, test_cbuf.item_count);
}

/* Test: cbuf2_peek_rgn at exact buffer size */
static void test_cbuf2_peek_rgn_exact_size(void)
{
    error_code rc;
    char out_buf[1000];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    test_cbuf.item_count = 1000;
    memset(test_cbuf.buf, 'E', 1000);

    rc = cbuf2_peek_rgn(&test_cbuf, out_buf, 0, 1000);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL('E', out_buf[0]);
    TEST_ASSERT_EQUAL('E', out_buf[999]);
}

/* Test: cbuf2_peek_rgn with non-zero start offset */
static void test_cbuf2_peek_rgn_with_offset(void)
{
    error_code rc;
    char out_buf[100];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    test_cbuf.item_count = 1000;
    memset(test_cbuf.buf, 'N', 500);
    memset(test_cbuf.buf + 500, 'O', 500);

    /* Read 100 bytes starting at offset 450 */
    rc = cbuf2_peek_rgn(&test_cbuf, out_buf, 450, 100);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL('N', out_buf[0]);   /* From first half */
    TEST_ASSERT_EQUAL('O', out_buf[99]);  /* From second half */
}

/* Test: Content type AAC variation */
static void test_cbuf2_insert_chunk_aac_content(void)
{
    error_code rc;
    char data[1024];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_AAC, 0, 2048, 8);

    memset(data, 'A', sizeof(data));

    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, data, 1024, CONTENT_TYPE_AAC, NULL);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1024, test_cbuf.item_count);
}

/* Test: cbuf2_write_index at various buffer positions */
static void test_cbuf2_write_index_various_positions(void)
{
    u_long write_idx;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Empty buffer */
    write_idx = cbuf2_write_index(&test_cbuf);
    TEST_ASSERT_EQUAL(0, write_idx);

    /* Half full */
    test_cbuf.item_count = 5120;
    write_idx = cbuf2_write_index(&test_cbuf);
    TEST_ASSERT_EQUAL(5120, write_idx);

    /* Full buffer */
    test_cbuf.item_count = 10240;
    write_idx = cbuf2_write_index(&test_cbuf);
    TEST_ASSERT_EQUAL(0, write_idx);  /* Wraps to start */
}

/* ============================================================================
 * cbuf2_extract_relay tests
 * ============================================================================ */

/* Test: cbuf2_extract_relay for MP3 content */
static void test_cbuf2_extract_relay_mp3_basic(void)
{
    error_code rc;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 0;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 1024, 10);

    /* Fill buffer with test data */
    test_cbuf.item_count = 5000;
    memset(test_cbuf.buf, 'D', 5000);

    /* Initialize relay entry first */
    rc = cbuf2_init_relay_entry(&test_cbuf, &relay, 2000);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_NOT_NULL(relay.m_buffer);

    /* Now extract relay data */
    rc = cbuf2_extract_relay(&test_cbuf, &relay);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1024, relay.m_left_to_send);  /* One chunk */
    TEST_ASSERT_EQUAL('D', relay.m_buffer[0]);

    /* Clean up */
    free(relay.m_buffer);
}

/* Test: cbuf2_extract_relay for MP3 with ICY metadata */
static void test_cbuf2_extract_relay_mp3_with_icy_metadata(void)
{
    error_code rc;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 1;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 1024, 10);

    /* Fill buffer with test data */
    test_cbuf.item_count = 5000;
    memset(test_cbuf.buf, 'M', 5000);

    /* Initialize relay entry first */
    rc = cbuf2_init_relay_entry(&test_cbuf, &relay, 3000);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Now extract relay data - should include metadata byte */
    rc = cbuf2_extract_relay(&test_cbuf, &relay);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    /* len should be chunk_size + 1 (for null metadata) */
    TEST_ASSERT_EQUAL(1025, relay.m_left_to_send);
    TEST_ASSERT_EQUAL('M', relay.m_buffer[0]);
    /* Last byte should be null metadata indicator */
    TEST_ASSERT_EQUAL('\0', relay.m_buffer[1024]);

    /* Clean up */
    free(relay.m_buffer);
}

/* Test: cbuf2_extract_relay returns error when buffer empty */
static void test_cbuf2_extract_relay_mp3_buffer_empty(void)
{
    error_code rc;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 0;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 1024, 10);

    /* Small amount of data, not enough for a chunk */
    test_cbuf.item_count = 500;

    /* Initialize relay entry */
    rc = cbuf2_init_relay_entry(&test_cbuf, &relay, 100);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Try to extract - should fail because offset + chunk_size > item_count */
    rc = cbuf2_extract_relay(&test_cbuf, &relay);

    TEST_ASSERT_EQUAL(SR_ERROR_BUFFER_EMPTY, rc);

    /* Clean up */
    free(relay.m_buffer);
}

/* Test: cbuf2_extract_relay for OGG content */
static void test_cbuf2_extract_relay_ogg_basic(void)
{
    error_code rc;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 0;
    relay.m_header_buf_ptr = NULL;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 1, 1024, 10);

    /* Fill buffer with test data */
    test_cbuf.item_count = 5000;
    memset(test_cbuf.buf, 'O', 5000);

    /* Set up relay manually for OGG testing (simpler than full init) */
    relay.m_buffer = (char *)malloc(2048);
    relay.m_buffer_size = 2048;
    relay.m_cbuf_offset = 0;

    /* Now extract relay data */
    rc = cbuf2_extract_relay(&test_cbuf, &relay);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL('O', relay.m_buffer[0]);

    /* Clean up */
    free(relay.m_buffer);
}

/* Test: cbuf2_extract_relay_ogg with header buffer */
static void test_cbuf2_extract_relay_ogg_with_header_buf(void)
{
    error_code rc;
    RELAY_LIST relay;
    char header_data[100];

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 0;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 1, 1024, 10);

    /* Set up header buffer */
    memset(header_data, 'H', sizeof(header_data));
    relay.m_header_buf_ptr = header_data;
    relay.m_header_buf_len = 100;
    relay.m_header_buf_off = 0;
    relay.m_buffer = (char *)malloc(200);
    relay.m_buffer_size = 200;

    /* Extract - should copy from header buffer first */
    rc = cbuf2_extract_relay(&test_cbuf, &relay);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(100, relay.m_left_to_send);
    TEST_ASSERT_EQUAL('H', relay.m_buffer[0]);
    TEST_ASSERT_NULL(relay.m_header_buf_ptr);  /* Should be cleared */

    /* Clean up */
    free(relay.m_buffer);
}

/* Test: cbuf2_extract_relay_ogg with large header buffer (requires multiple extracts) */
static void test_cbuf2_extract_relay_ogg_large_header(void)
{
    error_code rc;
    RELAY_LIST relay;
    static char header_data[500];

    memset(&relay, 0, sizeof(RELAY_LIST));

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 1, 1024, 10);

    /* Set up large header buffer */
    memset(header_data, 'L', sizeof(header_data));
    relay.m_header_buf_ptr = header_data;
    relay.m_header_buf_len = 500;
    relay.m_header_buf_off = 0;
    relay.m_buffer = (char *)malloc(200);
    relay.m_buffer_size = 200;

    /* First extract - should get first 200 bytes */
    rc = cbuf2_extract_relay(&test_cbuf, &relay);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(200, relay.m_left_to_send);
    TEST_ASSERT_EQUAL(200, relay.m_header_buf_off);
    TEST_ASSERT_EQUAL('L', relay.m_buffer[0]);
    TEST_ASSERT_NOT_NULL(relay.m_header_buf_ptr);  /* Still more to send */

    /* Clean up */
    free(relay.m_buffer);
}

/* Test: cbuf2_extract_relay_ogg from cbuf (no header buffer) */
static void test_cbuf2_extract_relay_ogg_from_cbuf(void)
{
    error_code rc;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_header_buf_ptr = NULL;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 1, 1024, 10);

    /* Fill buffer with test data */
    test_cbuf.item_count = 5000;
    memset(test_cbuf.buf, 'C', 5000);

    /* Set up relay to read from cbuf */
    relay.m_buffer = (char *)malloc(512);
    relay.m_buffer_size = 512;
    relay.m_cbuf_offset = 1000;  /* Start reading from offset 1000 */

    /* Extract from cbuf */
    rc = cbuf2_extract_relay(&test_cbuf, &relay);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(512, relay.m_left_to_send);
    TEST_ASSERT_EQUAL('C', relay.m_buffer[0]);
    TEST_ASSERT_EQUAL(1512, relay.m_cbuf_offset);  /* Advanced by buffer_size */

    /* Clean up */
    free(relay.m_buffer);
}

/* Test: cbuf2_extract_relay_ogg from cbuf with remaining < buffer_size */
static void test_cbuf2_extract_relay_ogg_partial_read(void)
{
    error_code rc;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_header_buf_ptr = NULL;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 1, 1024, 10);

    /* Fill buffer with test data */
    test_cbuf.item_count = 2000;
    memset(test_cbuf.buf, 'P', 2000);

    /* Set up relay to read remaining data (less than buffer_size) */
    relay.m_buffer = (char *)malloc(1024);
    relay.m_buffer_size = 1024;
    relay.m_cbuf_offset = 1500;  /* Only 500 bytes remaining */

    /* Extract from cbuf */
    rc = cbuf2_extract_relay(&test_cbuf, &relay);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(500, relay.m_left_to_send);  /* Got only remaining */
    TEST_ASSERT_EQUAL('P', relay.m_buffer[0]);
    TEST_ASSERT_EQUAL(2000, relay.m_cbuf_offset);

    /* Clean up */
    free(relay.m_buffer);
}

/* Test: cbuf2_extract_relay_ogg buffer empty */
static void test_cbuf2_extract_relay_ogg_buffer_empty(void)
{
    error_code rc;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_header_buf_ptr = NULL;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 1, 1024, 10);

    /* Buffer has some data */
    test_cbuf.item_count = 1000;

    /* Set up relay at end of data */
    relay.m_buffer = (char *)malloc(512);
    relay.m_buffer_size = 512;
    relay.m_cbuf_offset = 1000;  /* At end, nothing remaining */

    /* Extract should fail */
    rc = cbuf2_extract_relay(&test_cbuf, &relay);

    TEST_ASSERT_EQUAL(SR_ERROR_BUFFER_EMPTY, rc);

    /* Clean up */
    free(relay.m_buffer);
}

/* ============================================================================
 * cbuf2_extract with relay tests
 * ============================================================================ */

/* Test: cbuf2_extract with have_relay enabled but NULL relay_list */
static void test_cbuf2_extract_with_relay_null_list(void)
{
    error_code rc;
    char extract_buf[1024];
    u_long curr_song;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 1024, 10);

    /* Add test data */
    test_cbuf.item_count = 2048;
    memset(test_cbuf.buf, 'R', 2048);

    /* relay_list is NULL (set in setUp) */
    test_rmi.relay_list = NULL;

    rc = cbuf2_extract(&test_rmi, &test_cbuf, extract_buf, 1024, &curr_song);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1024, test_cbuf.item_count);
    TEST_ASSERT_EQUAL('R', extract_buf[0]);
}

/* Test: cbuf2_extract with relay and active relay client */
static void test_cbuf2_extract_with_relay_active_client(void)
{
    error_code rc;
    char extract_buf[1024];
    u_long curr_song;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 1024, 10);

    /* Add test data */
    test_cbuf.item_count = 3000;
    memset(test_cbuf.buf, 'A', 3000);

    /* Set up relay client */
    relay.m_is_new = 0;  /* Not new, so it will be checked */
    relay.m_cbuf_offset = 2000;  /* Offset into buffer */
    relay.m_next = NULL;
    test_rmi.relay_list = &relay;

    rc = cbuf2_extract(&test_rmi, &test_cbuf, extract_buf, 1024, &curr_song);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    /* Relay offset should be adjusted */
    TEST_ASSERT_EQUAL(976, relay.m_cbuf_offset);  /* 2000 - 1024 */

    /* Clean up */
    test_rmi.relay_list = NULL;
}

/* Test: cbuf2_extract with relay client that can't keep up */
static void test_cbuf2_extract_relay_client_disconnect(void)
{
    error_code rc;
    char extract_buf[1024];
    u_long curr_song;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 1024, 10);

    /* Add test data */
    test_cbuf.item_count = 3000;
    memset(test_cbuf.buf, 'X', 3000);

    /* Set up relay client with small offset (can't keep up) */
    relay.m_is_new = 0;
    relay.m_cbuf_offset = 500;  /* Less than extract count (1024) */
    relay.m_next = NULL;
    test_rmi.relay_list = &relay;

    rc = cbuf2_extract(&test_rmi, &test_cbuf, extract_buf, 1024, &curr_song);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    /* relaylib_disconnect should have been called (stubbed) */

    /* Clean up */
    test_rmi.relay_list = NULL;
}

/* Test: cbuf2_extract with new relay client (skip update) */
static void test_cbuf2_extract_relay_new_client(void)
{
    error_code rc;
    char extract_buf[1024];
    u_long curr_song;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 1024, 10);

    /* Add test data */
    test_cbuf.item_count = 3000;
    memset(test_cbuf.buf, 'N', 3000);

    /* Set up new relay client */
    relay.m_is_new = 1;  /* New, so it will be skipped */
    relay.m_cbuf_offset = 1500;
    relay.m_next = NULL;
    test_rmi.relay_list = &relay;

    rc = cbuf2_extract(&test_rmi, &test_cbuf, extract_buf, 1024, &curr_song);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    /* Offset should remain unchanged for new client */
    TEST_ASSERT_EQUAL(1500, relay.m_cbuf_offset);

    /* Clean up */
    test_rmi.relay_list = NULL;
}

/* ============================================================================
 * cbuf2_extract with metadata tests
 * ============================================================================ */

/* Test: cbuf2_extract with metadata in list */
static void test_cbuf2_extract_with_metadata(void)
{
    error_code rc;
    char extract_buf[1024];
    char insert_data[1024];
    u_long curr_song;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Insert chunk with track info to add metadata */
    memset(insert_data, 'T', sizeof(insert_data));
    test_ti.have_track_info = 1;
    memset(test_ti.composed_metadata, 0, sizeof(test_ti.composed_metadata));
    test_ti.composed_metadata[0] = 1;  /* Length indicator */
    strcpy(&test_ti.composed_metadata[1], "TestMetadata");

    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, insert_data, 1024, CONTENT_TYPE_MP3, &test_ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Insert another chunk */
    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, insert_data, 1024, CONTENT_TYPE_MP3, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Extract should advance metadata list */
    rc = cbuf2_extract(&test_rmi, &test_cbuf, extract_buf, 1024, &curr_song);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
}

/* ============================================================================
 * cbuf2_peek_rgn wrap-around tests
 * ============================================================================ */

/* Test: cbuf2_peek_rgn with actual wrap-around read */
static void test_cbuf2_peek_rgn_actual_wrap(void)
{
    error_code rc;
    char out_buf[500];

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Set up wrapped buffer state */
    test_cbuf.base_idx = 9800;
    test_cbuf.item_count = 600;

    /* Fill data: 9800-10239 = 'A' (440 bytes), 0-159 = 'B' (160 bytes) */
    memset(test_cbuf.buf + 9800, 'A', 440);
    memset(test_cbuf.buf, 'B', 160);

    /* Read that spans the wrap point */
    /* start_offset = 400, length = 200
     * sidx = (9800 + 400) % 10240 = 10200
     * first_hunk = 10240 - 10200 = 40 bytes of 'A'
     * then 160 bytes from start = 'B' */
    rc = cbuf2_peek_rgn(&test_cbuf, out_buf, 400, 200);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL('A', out_buf[0]);    /* First 40 bytes are 'A' */
    TEST_ASSERT_EQUAL('A', out_buf[39]);   /* Last of first hunk */
    TEST_ASSERT_EQUAL('B', out_buf[40]);   /* First of wrapped portion */
    TEST_ASSERT_EQUAL('B', out_buf[199]);  /* Last byte */
}

/* ============================================================================
 * cbuf2_advance_ogg advanced tests
 * ============================================================================ */

/* Test: cbuf2_advance_ogg needs to free space with relay */
static void test_cbuf2_advance_ogg_with_relay(void)
{
    error_code rc;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 1, 1024, 10);

    /* Fill buffer nearly full */
    test_cbuf.item_count = 10000;

    /* With have_relay = 1, should acquire semaphores */
    rc = cbuf2_advance_ogg(&test_rmi, &test_cbuf, 500);

    /* Should succeed after advancing */
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
}

/* ============================================================================
 * cbuf2_extract_relay_mp3 with metadata tests
 * ============================================================================ */

/* Test: cbuf2_extract_relay_mp3 with matching metadata */
static void test_cbuf2_extract_relay_mp3_with_matching_metadata(void)
{
    error_code rc;
    RELAY_LIST relay;
    char insert_data[1024];
    METADATA_LIST *ml;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 1;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 1, 1024, 10);

    /* Insert chunk with metadata */
    memset(insert_data, 'Z', sizeof(insert_data));
    test_ti.have_track_info = 1;
    memset(test_ti.composed_metadata, 0, sizeof(test_ti.composed_metadata));
    test_ti.composed_metadata[0] = 1;  /* 16 bytes of metadata */
    strcpy(&test_ti.composed_metadata[1], "Track1");

    rc = cbuf2_insert_chunk(&test_rmi, &test_cbuf, insert_data, 1024, CONTENT_TYPE_MP3, &test_ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Initialize relay at the start */
    rc = cbuf2_init_relay_entry(&test_cbuf, &relay, 2000);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);

    /* Verify metadata list is not empty */
    TEST_ASSERT_FALSE(list_empty(&test_cbuf.metadata_list));

    /* Extract relay - should include metadata */
    rc = cbuf2_extract_relay(&test_cbuf, &relay);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    /* len should be chunk_size + metadata length (16+1=17) */
    TEST_ASSERT_EQUAL(1041, relay.m_left_to_send);  /* 1024 + 17 */

    /* Clean up */
    free(relay.m_buffer);
    /* Clean up metadata list */
    while (!list_empty(&test_cbuf.metadata_list)) {
        LIST *p = test_cbuf.metadata_list.next;
        ml = list_entry(p, METADATA_LIST, m_list);
        list_del(p);
        free(ml);
    }
}

/* ============================================================================
 * OGG page handling tests
 * ============================================================================ */

/* Helper function to create an OGG page and add to list */
static OGG_PAGE_LIST* create_ogg_page(u_long start, u_long len, int flags)
{
    OGG_PAGE_LIST *page = (OGG_PAGE_LIST *)malloc(sizeof(OGG_PAGE_LIST));
    page->m_page_start = start;
    page->m_page_len = len;
    page->m_page_flags = flags;
    page->m_header_buf_ptr = NULL;
    page->m_header_buf_len = 0;
    INIT_LIST_HEAD(&page->m_list);
    return page;
}

/* Test: cbuf2_ogg_peek_song with actual pages */
static void test_cbuf2_ogg_peek_song_with_pages(void)
{
    error_code rc;
    char out_buf[256];
    unsigned long amt_filled;
    int eos;
    OGG_PAGE_LIST *page1, *page2;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 0, 1024, 10);

    /* Fill buffer with data */
    test_cbuf.item_count = 2000;
    memset(test_cbuf.buf, 'G', 2000);

    /* Create OGG pages */
    page1 = create_ogg_page(0, 500, 0);  /* First page, no special flags */
    page2 = create_ogg_page(500, 500, 0);  /* Second page */

    /* Add pages to list */
    list_add_tail(&page1->m_list, &test_cbuf.ogg_page_list);
    list_add_tail(&page2->m_list, &test_cbuf.ogg_page_list);

    /* First peek - should initialize song_page */
    rc = cbuf2_ogg_peek_song(&test_cbuf, out_buf, 256, &amt_filled, &eos);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(256, amt_filled);
    TEST_ASSERT_EQUAL(0, eos);
    TEST_ASSERT_EQUAL('G', out_buf[0]);

    /* Peek again to continue reading */
    rc = cbuf2_ogg_peek_song(&test_cbuf, out_buf, 256, &amt_filled, &eos);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(244, amt_filled);  /* Remaining from first page */

    /* Clean up pages */
    list_del(&page1->m_list);
    list_del(&page2->m_list);
    free(page1);
    free(page2);
}

/* Test: cbuf2_ogg_peek_song with EOS page */
static void test_cbuf2_ogg_peek_song_with_eos(void)
{
    error_code rc;
    char out_buf[1024];
    unsigned long amt_filled;
    int eos;
    OGG_PAGE_LIST *page;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 0, 1024, 10);

    /* Fill buffer with data */
    test_cbuf.item_count = 500;
    memset(test_cbuf.buf, 'E', 500);

    /* Create EOS page */
    page = create_ogg_page(0, 500, OGG_PAGE_EOS);

    list_add_tail(&page->m_list, &test_cbuf.ogg_page_list);

    /* Peek - should get EOS flag */
    rc = cbuf2_ogg_peek_song(&test_cbuf, out_buf, 1024, &amt_filled, &eos);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(500, amt_filled);
    TEST_ASSERT_EQUAL(1, eos);

    /* Clean up */
    list_del(&page->m_list);
    free(page);
}

/* Test: cbuf2_ogg_peek_song advancing to next page */
static void test_cbuf2_ogg_peek_song_advance_page(void)
{
    error_code rc;
    char out_buf[600];
    unsigned long amt_filled;
    int eos;
    OGG_PAGE_LIST *page1, *page2;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 0, 1024, 10);

    /* Fill buffer with data */
    test_cbuf.item_count = 1000;
    memset(test_cbuf.buf, 'A', 500);
    memset(test_cbuf.buf + 500, 'B', 500);

    /* Create two pages */
    page1 = create_ogg_page(0, 500, 0);
    page2 = create_ogg_page(500, 500, 0);

    list_add_tail(&page1->m_list, &test_cbuf.ogg_page_list);
    list_add_tail(&page2->m_list, &test_cbuf.ogg_page_list);

    /* Read entire first page */
    rc = cbuf2_ogg_peek_song(&test_cbuf, out_buf, 500, &amt_filled, &eos);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(500, amt_filled);

    /* Now read from second page (first page should be marked done) */
    rc = cbuf2_ogg_peek_song(&test_cbuf, out_buf, 500, &amt_filled, &eos);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(500, amt_filled);
    TEST_ASSERT_EQUAL('B', out_buf[0]);  /* From second page */

    /* Clean up */
    list_del(&page1->m_list);
    list_del(&page2->m_list);
    free(page1);
    free(page2);
}

/* Test: cbuf2_ogg_peek_song at end of list */
static void test_cbuf2_ogg_peek_song_end_of_list(void)
{
    error_code rc;
    char out_buf[256];
    unsigned long amt_filled;
    int eos;
    OGG_PAGE_LIST *page;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 0, 1024, 10);

    /* Fill buffer */
    test_cbuf.item_count = 200;
    memset(test_cbuf.buf, 'Z', 200);

    /* Create single small page */
    page = create_ogg_page(0, 200, 0);
    list_add_tail(&page->m_list, &test_cbuf.ogg_page_list);

    /* Read entire page */
    rc = cbuf2_ogg_peek_song(&test_cbuf, out_buf, 256, &amt_filled, &eos);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(200, amt_filled);

    /* Try to read more - should return with 0 filled */
    rc = cbuf2_ogg_peek_song(&test_cbuf, out_buf, 256, &amt_filled, &eos);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(0, amt_filled);

    /* Clean up */
    list_del(&page->m_list);
    free(page);
}

/* ============================================================================
 * cbuf2_init_relay_entry OGG tests
 * ============================================================================ */

/* Test: cbuf2_init_relay_entry for OGG - no pages returns error */
static void test_cbuf2_init_relay_entry_ogg_no_pages(void)
{
    error_code rc;
    RELAY_LIST relay;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 0;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 1, 1024, 10);
    test_cbuf.item_count = 5000;

    /* OGG with empty page list should return error */
    rc = cbuf2_init_relay_entry(&test_cbuf, &relay, 2000);

    TEST_ASSERT_EQUAL(SR_ERROR_NO_OGG_PAGES_FOR_RELAY, rc);
}

/* Test: cbuf2_init_relay_entry for OGG with BOS page */
static void test_cbuf2_init_relay_entry_ogg_with_bos_page(void)
{
    error_code rc;
    RELAY_LIST relay;
    OGG_PAGE_LIST *page;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 0;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 1, 1024, 10);
    test_cbuf.item_count = 5000;
    memset(test_cbuf.buf, 'O', 5000);

    /* Create BOS page */
    page = create_ogg_page(1000, 500, OGG_PAGE_BOS);
    list_add_tail(&page->m_list, &test_cbuf.ogg_page_list);

    rc = cbuf2_init_relay_entry(&test_cbuf, &relay, 2000);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1000, relay.m_cbuf_offset);  /* Should be at page start */
    TEST_ASSERT_NULL(relay.m_header_buf_ptr);
    TEST_ASSERT_NOT_NULL(relay.m_buffer);

    /* Clean up */
    free(relay.m_buffer);
    list_del(&page->m_list);
    free(page);
}

/* Test: cbuf2_init_relay_entry for OGG with regular page and header buf */
static void test_cbuf2_init_relay_entry_ogg_with_header(void)
{
    error_code rc;
    RELAY_LIST relay;
    OGG_PAGE_LIST *page;
    static char header_data[100];

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 0;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 1, 1024, 10);
    test_cbuf.item_count = 5000;
    memset(test_cbuf.buf, 'H', 5000);

    /* Create regular page (not BOS, not OGG_PAGE_2) with header buffer */
    memset(header_data, 'X', sizeof(header_data));
    page = create_ogg_page(2000, 500, 0);  /* No flags, so uses header path */
    page->m_header_buf_ptr = header_data;
    page->m_header_buf_len = 100;
    list_add_tail(&page->m_list, &test_cbuf.ogg_page_list);

    rc = cbuf2_init_relay_entry(&test_cbuf, &relay, 3000);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(2000, relay.m_cbuf_offset);
    TEST_ASSERT_EQUAL(header_data, relay.m_header_buf_ptr);
    TEST_ASSERT_EQUAL(100, relay.m_header_buf_len);
    TEST_ASSERT_NOT_NULL(relay.m_buffer);

    /* Clean up */
    free(relay.m_buffer);
    list_del(&page->m_list);
    free(page);
}

/* Test: cbuf2_init_relay_entry for OGG skips OGG_PAGE_2 */
static void test_cbuf2_init_relay_entry_ogg_skips_page2(void)
{
    error_code rc;
    RELAY_LIST relay;
    OGG_PAGE_LIST *page1, *page2;

    memset(&relay, 0, sizeof(RELAY_LIST));
    relay.m_icy_metadata = 0;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 1, 1024, 10);
    test_cbuf.item_count = 5000;

    /* Create OGG_PAGE_2 page (should be skipped) */
    page1 = create_ogg_page(500, 500, OGG_PAGE_2);
    /* Create regular BOS page after */
    page2 = create_ogg_page(1000, 500, OGG_PAGE_BOS);

    list_add_tail(&page1->m_list, &test_cbuf.ogg_page_list);
    list_add_tail(&page2->m_list, &test_cbuf.ogg_page_list);

    rc = cbuf2_init_relay_entry(&test_cbuf, &relay, 4000);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    /* Should use page2 (BOS), not page1 (PAGE_2) */
    TEST_ASSERT_EQUAL(1000, relay.m_cbuf_offset);

    /* Clean up */
    free(relay.m_buffer);
    list_del(&page1->m_list);
    list_del(&page2->m_list);
    free(page1);
    free(page2);
}

/* ============================================================================
 * cbuf2_advance_ogg with page deletion tests
 * ============================================================================ */

/* Test: cbuf2_advance_ogg removes pages when advancing */
static void test_cbuf2_advance_ogg_removes_pages(void)
{
    error_code rc;
    OGG_PAGE_LIST *page1, *page2;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 0, 1024, 10);

    /* Fill buffer */
    test_cbuf.item_count = 9500;
    test_cbuf.base_idx = 0;

    /* Create pages */
    page1 = create_ogg_page(0, 500, 0);
    page2 = create_ogg_page(500, 500, 0);
    list_add_tail(&page1->m_list, &test_cbuf.ogg_page_list);
    list_add_tail(&page2->m_list, &test_cbuf.ogg_page_list);

    /* Request more space than available - should advance and remove pages */
    rc = cbuf2_advance_ogg(&test_rmi, &test_cbuf, 1000);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    /* Pages should have been removed since they were in freed region */
    /* Note: The actual pages are freed inside cbuf2_advance_ogg, so list should be shorter */
}

/* Test: cbuf2_advance_ogg with EOS page (needs to free header buffer) */
static void test_cbuf2_advance_ogg_eos_page_cleanup(void)
{
    error_code rc;
    OGG_PAGE_LIST *page;
    char *header_buf;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 0, 1024, 10);

    /* Fill buffer */
    test_cbuf.item_count = 9500;
    test_cbuf.base_idx = 0;

    /* Create EOS page with header buffer */
    header_buf = (char *)malloc(100);
    page = create_ogg_page(0, 500, OGG_PAGE_EOS);
    page->m_header_buf_ptr = header_buf;
    list_add_tail(&page->m_list, &test_cbuf.ogg_page_list);

    /* Request more space - should remove EOS page and free header */
    rc = cbuf2_advance_ogg(&test_rmi, &test_cbuf, 1000);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    /* header_buf should have been freed inside the function */
}

/* Test: cbuf2_advance_ogg with page beyond count threshold */
static void test_cbuf2_advance_ogg_page_beyond_threshold(void)
{
    error_code rc;
    OGG_PAGE_LIST *page;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_OGG, 0, 1024, 10);

    /* Fill buffer mostly */
    test_cbuf.item_count = 9000;
    test_cbuf.base_idx = 0;

    /* Create a page far into the buffer (beyond what we need to free).
     * This exercises the "pos > count" branch. */
    page = create_ogg_page(5000, 500, 0);  /* Page at offset 5000 */
    list_add_tail(&page->m_list, &test_cbuf.ogg_page_list);

    /* Request 2000 free - need to free 2000 - (10240 - 9000) = 760 bytes
     * Page at 5000 is beyond 760, so it won't be removed */
    rc = cbuf2_advance_ogg(&test_rmi, &test_cbuf, 2000);

    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    /* Page should still be in the list */
    TEST_ASSERT_FALSE(list_empty(&test_cbuf.ogg_page_list));

    /* Clean up */
    list_del(&page->m_list);
    free(page);
}

/* ============================================================================
 * cbuf2_get_free_tail edge cases
 * ============================================================================ */

/* Test: cbuf2_get_free_tail when free space is less than tail space */
static void test_cbuf2_get_free_tail_free_less_than_tail(void)
{
    u_long free_tail;

    cbuf2_init(&test_cbuf, CONTENT_TYPE_MP3, 0, 1024, 10);

    /* Buffer almost full, write index near start */
    test_cbuf.base_idx = 100;
    test_cbuf.item_count = 10000;
    /* write_idx = (100 + 10000) % 10240 = 10100 - 10240 = -140 -> 9860 wrong
     * Actually: 10100 % 10240 = 10100 (doesn't wrap yet)
     * Let me recalculate: base=100, count=10000
     * write_idx = cbuf2_add(100, 10000) = 100 + 10000 = 10100
     * Since 10100 >= 10240, write_idx = 10100 - 10240 = -140 -> doesn't work that way
     * In cbuf2_add: pos = 100 + 10000 = 10100, if 10100 >= 10240, pos -= 10240
     * 10100 >= 10240? No, so write_idx = 10100 -- wait that's out of bounds
     * Hmm, if size is 10240 and we're at 10100, that's still within bounds (0-10239)
     * 10100 is valid index. free = 10240 - 10000 = 240
     * free_tail = 10240 - 10100 = 140
     * Since ft (140) < fr (240), return ft = 140 */

    /* Wait, index 10100 is out of bounds for size 10240 (valid: 0-10239)
     * Let me re-check cbuf2_add:
     *   pos += len; if (pos >= size) pos -= size; return pos;
     * So: pos = 100 + 10000 = 10100; 10100 >= 10240? No (10100 < 10240)
     * So write_idx = 10100 -- but that's > 10239 which is invalid!
     * That seems like a bug... or I'm misunderstanding the size.
     * size = 10240, valid indices = 0 to 10239
     * Ah wait, let me reread: if pos >= size, pos -= size
     * 10100 < 10240, so no subtraction. But 10100 is still a valid index?
     * No, size is 10240, so indices are 0-10239. 10100 is valid.
     * Oh I see my mistake: 10240 total bytes means indices 0-10239, so 10100 is valid!
     * Then free_tail = 10240 - 10100 = 140
     * free = 10240 - 10000 = 240
     * Since 140 < 240, return 140 */

    free_tail = cbuf2_get_free_tail(&test_cbuf);

    /* ft = 10240 - write_idx = 10240 - 10100 = 140 */
    /* fr = 10240 - 10000 = 240 */
    /* Since ft < fr, return ft = 140 */
    TEST_ASSERT_EQUAL(140, free_tail);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* cbuf2_init tests */
    RUN_TEST(test_cbuf2_init_valid_params);
    RUN_TEST(test_cbuf2_init_zero_chunk_size);
    RUN_TEST(test_cbuf2_init_zero_num_chunks);
    RUN_TEST(test_cbuf2_init_with_relay);
    RUN_TEST(test_cbuf2_init_ogg_content);

    /* cbuf2_get_free tests */
    RUN_TEST(test_cbuf2_get_free_empty_buffer);
    RUN_TEST(test_cbuf2_get_free_full_buffer);
    RUN_TEST(test_cbuf2_get_free_partial_buffer);

    /* cbuf2_write_index tests */
    RUN_TEST(test_cbuf2_write_index_empty);
    RUN_TEST(test_cbuf2_write_index_with_data);
    RUN_TEST(test_cbuf2_write_index_wrap_around);

    /* cbuf2_get_free_tail tests */
    RUN_TEST(test_cbuf2_get_free_tail_empty);
    RUN_TEST(test_cbuf2_get_free_tail_near_end);

    /* cbuf2_set_next_song tests */
    RUN_TEST(test_cbuf2_set_next_song);
    RUN_TEST(test_cbuf2_set_next_song_update);

    /* cbuf2_peek tests */
    RUN_TEST(test_cbuf2_peek_empty_buffer);
    RUN_TEST(test_cbuf2_peek_request_too_much);
    RUN_TEST(test_cbuf2_peek_success);

    /* cbuf2_fastforward tests */
    RUN_TEST(test_cbuf2_fastforward_empty_buffer);
    RUN_TEST(test_cbuf2_fastforward_success);
    RUN_TEST(test_cbuf2_fastforward_wrap);

    /* cbuf2_destroy tests */
    RUN_TEST(test_cbuf2_destroy_cleans_up);
    RUN_TEST(test_cbuf2_destroy_null_buffer);

    /* Buffer state tests */
    RUN_TEST(test_cbuf2_empty_state);
    RUN_TEST(test_cbuf2_full_state);
    RUN_TEST(test_cbuf2_aac_content_type);

    /* cbuf2_insert_chunk tests */
    RUN_TEST(test_cbuf2_insert_chunk_basic);
    RUN_TEST(test_cbuf2_insert_chunk_with_track_info);
    RUN_TEST(test_cbuf2_insert_chunk_buffer_full);

    /* cbuf2_peek_rgn tests */
    RUN_TEST(test_cbuf2_peek_rgn_basic);
    RUN_TEST(test_cbuf2_peek_rgn_too_long);
    RUN_TEST(test_cbuf2_peek_rgn_wrap_around);
    RUN_TEST(test_cbuf2_peek_rgn_exact_size);
    RUN_TEST(test_cbuf2_peek_rgn_with_offset);

    /* cbuf2_extract tests */
    RUN_TEST(test_cbuf2_extract_basic);
    RUN_TEST(test_cbuf2_extract_insufficient_data);
    RUN_TEST(test_cbuf2_extract_wrap_around);
    RUN_TEST(test_cbuf2_extract_next_song_update);
    RUN_TEST(test_cbuf2_extract_next_song_reset);

    /* cbuf2_peek wrap-around tests */
    RUN_TEST(test_cbuf2_peek_wrap_around);

    /* cbuf2_insert_chunk edge cases */
    RUN_TEST(test_cbuf2_insert_chunk_multiple);
    RUN_TEST(test_cbuf2_insert_chunk_wrap_around);
    RUN_TEST(test_cbuf2_insert_chunk_ogg_content);
    RUN_TEST(test_cbuf2_insert_chunk_aac_content);

    /* cbuf2_get_free/free_tail edge cases */
    RUN_TEST(test_cbuf2_get_free_quarter_full);
    RUN_TEST(test_cbuf2_get_free_tail_after_wrap);

    /* cbuf2_advance_ogg tests */
    RUN_TEST(test_cbuf2_advance_ogg_sufficient_space);
    RUN_TEST(test_cbuf2_advance_ogg_empty_page_list);

    /* cbuf2_init_relay_entry tests */
    RUN_TEST(test_cbuf2_init_relay_entry_mp3_no_metadata);
    RUN_TEST(test_cbuf2_init_relay_entry_mp3_with_metadata);
    RUN_TEST(test_cbuf2_init_relay_entry_burst_exceeds_count);
    RUN_TEST(test_cbuf2_init_relay_entry_chunk_alignment);

    /* cbuf2_ogg_peek_song tests */
    RUN_TEST(test_cbuf2_ogg_peek_song_empty_list);

    /* Edge case and stress tests */
    RUN_TEST(test_cbuf2_multiple_operations_sequence);
    RUN_TEST(test_cbuf2_small_buffer);
    RUN_TEST(test_cbuf2_large_buffer);
    RUN_TEST(test_cbuf2_fastforward_to_empty);
    RUN_TEST(test_cbuf2_fastforward_too_much);
    RUN_TEST(test_cbuf2_write_index_various_positions);

    /* cbuf2_extract_relay tests */
    RUN_TEST(test_cbuf2_extract_relay_mp3_basic);
    RUN_TEST(test_cbuf2_extract_relay_mp3_with_icy_metadata);
    RUN_TEST(test_cbuf2_extract_relay_mp3_buffer_empty);
    RUN_TEST(test_cbuf2_extract_relay_ogg_basic);
    RUN_TEST(test_cbuf2_extract_relay_ogg_with_header_buf);
    RUN_TEST(test_cbuf2_extract_relay_ogg_large_header);
    RUN_TEST(test_cbuf2_extract_relay_ogg_from_cbuf);
    RUN_TEST(test_cbuf2_extract_relay_ogg_partial_read);
    RUN_TEST(test_cbuf2_extract_relay_ogg_buffer_empty);

    /* cbuf2_extract with relay tests */
    RUN_TEST(test_cbuf2_extract_with_relay_null_list);
    RUN_TEST(test_cbuf2_extract_with_relay_active_client);
    RUN_TEST(test_cbuf2_extract_relay_client_disconnect);
    RUN_TEST(test_cbuf2_extract_relay_new_client);

    /* cbuf2_extract with metadata tests */
    RUN_TEST(test_cbuf2_extract_with_metadata);

    /* cbuf2_peek_rgn wrap-around tests */
    RUN_TEST(test_cbuf2_peek_rgn_actual_wrap);

    /* cbuf2_advance_ogg tests */
    RUN_TEST(test_cbuf2_advance_ogg_with_relay);

    /* cbuf2_extract_relay_mp3 with metadata tests */
    RUN_TEST(test_cbuf2_extract_relay_mp3_with_matching_metadata);

    /* OGG page handling tests */
    RUN_TEST(test_cbuf2_ogg_peek_song_with_pages);
    RUN_TEST(test_cbuf2_ogg_peek_song_with_eos);
    RUN_TEST(test_cbuf2_ogg_peek_song_advance_page);
    RUN_TEST(test_cbuf2_ogg_peek_song_end_of_list);

    /* cbuf2_init_relay_entry OGG tests */
    RUN_TEST(test_cbuf2_init_relay_entry_ogg_no_pages);
    RUN_TEST(test_cbuf2_init_relay_entry_ogg_with_bos_page);
    RUN_TEST(test_cbuf2_init_relay_entry_ogg_with_header);
    RUN_TEST(test_cbuf2_init_relay_entry_ogg_skips_page2);

    /* cbuf2_advance_ogg with page deletion tests */
    RUN_TEST(test_cbuf2_advance_ogg_removes_pages);
    RUN_TEST(test_cbuf2_advance_ogg_eos_page_cleanup);
    RUN_TEST(test_cbuf2_advance_ogg_page_beyond_threshold);

    /* cbuf2_get_free_tail edge cases */
    RUN_TEST(test_cbuf2_get_free_tail_free_less_than_tail);

    return UNITY_END();
}
