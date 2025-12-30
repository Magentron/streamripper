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

    return UNITY_END();
}
