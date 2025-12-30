/*
 * test_helpers.h - Common test utilities and macros for Streamripper tests
 *
 * This file provides common utilities, macros, and helper functions used
 * across unit, integration, and end-to-end tests.
 */

#ifndef __TEST_HELPERS_H__
#define __TEST_HELPERS_H__

#include <unity.h>
#include <string.h>
#include <stdlib.h>

/*
 * Path utilities
 */
#ifndef SR_MAX_PATH
#define SR_MAX_PATH 4096
#endif

/*
 * Test data directories
 */
#define TEST_FIXTURES_DIR "fixtures"
#define TEST_MP3_FRAMES_DIR "fixtures/mp3_frames"
#define TEST_OGG_PAGES_DIR "fixtures/ogg_pages"
#define TEST_AAC_FRAMES_DIR "fixtures/aac_frames"
#define TEST_HTTP_RESPONSES_DIR "fixtures/http_responses"
#define TEST_STREAMS_DIR "fixtures/streams"

/*
 * Common buffer sizes
 */
#define TEST_SMALL_BUFFER_SIZE 256
#define TEST_MEDIUM_BUFFER_SIZE 1024
#define TEST_LARGE_BUFFER_SIZE 8192

/*
 * Helper macro to safely allocate memory and fail test if allocation fails
 */
#define TEST_ALLOC(ptr, type, count) do { \
    (ptr) = (type *)malloc((count) * sizeof(type)); \
    TEST_ASSERT_NOT_NULL_MESSAGE((ptr), "Memory allocation failed"); \
} while(0)

/*
 * Helper macro to safely free memory and null the pointer
 */
#define TEST_FREE(ptr) do { \
    if ((ptr) != NULL) { \
        free((ptr)); \
        (ptr) = NULL; \
    } \
} while(0)

/*
 * Helper macro to assert two strings are equal, with better error messages
 */
#define TEST_ASSERT_STRINGS_EQUAL(expected, actual) \
    TEST_ASSERT_EQUAL_STRING_MESSAGE((expected), (actual), \
        "String mismatch")

/*
 * Helper macro to assert two memory regions are equal
 */
#define TEST_ASSERT_MEMORY_EQUAL(expected, actual, len) \
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE((expected), (actual), (len), \
        "Memory mismatch")

/*
 * Helper macro to assert error code equals SR_SUCCESS
 */
#define TEST_ASSERT_SUCCESS(result) \
    TEST_ASSERT_EQUAL_MESSAGE(SR_SUCCESS, (result), \
        "Expected SR_SUCCESS")

/*
 * Helper macro to assert error code equals specific error
 */
#define TEST_ASSERT_ERROR(expected_error, result) \
    TEST_ASSERT_EQUAL_MESSAGE((expected_error), (result), \
        "Error code mismatch")

/*
 * Helper function to create a temporary directory for test output
 * Returns 0 on success, -1 on failure
 */
static inline int test_create_temp_dir(char *path, size_t path_size)
{
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL) {
        tmpdir = "/tmp";
    }
    int ret = snprintf(path, path_size, "%s/sr_test_XXXXXX", tmpdir);
    if (ret < 0 || (size_t)ret >= path_size) {
        return -1;
    }
    if (mkdtemp(path) == NULL) {
        return -1;
    }
    return 0;
}

/*
 * Helper function to read entire file into buffer
 * Returns number of bytes read, or -1 on error
 * Caller must free the returned buffer
 */
static inline long test_read_file(const char *filename, char **buffer)
{
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return -1;
    }

    *buffer = (char *)malloc(size);
    if (*buffer == NULL) {
        fclose(f);
        return -1;
    }

    long bytes_read = fread(*buffer, 1, size, f);
    fclose(f);

    if (bytes_read != size) {
        free(*buffer);
        *buffer = NULL;
        return -1;
    }

    return size;
}

/*
 * Helper function to write buffer to file
 * Returns 0 on success, -1 on error
 */
static inline int test_write_file(const char *filename, const char *buffer, size_t size)
{
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        return -1;
    }

    size_t written = fwrite(buffer, 1, size, f);
    fclose(f);

    return (written == size) ? 0 : -1;
}

/*
 * Helper function to check if file exists
 * Returns 1 if exists, 0 otherwise
 */
static inline int test_file_exists(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (f != NULL) {
        fclose(f);
        return 1;
    }
    return 0;
}

/*
 * Helper function to get file size
 * Returns file size or -1 on error
 */
static inline long test_file_size(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);

    return size;
}

/*
 * Mock initialization helpers
 * These macros help ensure consistent mock setup/teardown
 */
#define INIT_MOCKS_BEGIN() do {
#define INIT_MOCKS_END() } while(0)

#define VERIFY_MOCKS_BEGIN() do {
#define VERIFY_MOCKS_END() } while(0)

/*
 * Test timing helpers
 */
#include <time.h>

typedef struct {
    clock_t start;
    clock_t end;
} test_timer_t;

static inline void test_timer_start(test_timer_t *timer)
{
    timer->start = clock();
}

static inline double test_timer_elapsed_ms(test_timer_t *timer)
{
    timer->end = clock();
    return ((double)(timer->end - timer->start) / CLOCKS_PER_SEC) * 1000.0;
}

/*
 * Test data generation helpers
 */
static inline void test_fill_buffer_pattern(char *buffer, size_t size, unsigned char pattern)
{
    memset(buffer, pattern, size);
}

static inline void test_fill_buffer_sequential(char *buffer, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        buffer[i] = (char)(i & 0xFF);
    }
}

/*
 * Common test fixture structure
 */
typedef struct {
    char temp_dir[SR_MAX_PATH];
    char *test_buffer;
    size_t test_buffer_size;
} test_fixture_t;

static inline int test_fixture_setup(test_fixture_t *fixture, size_t buffer_size)
{
    memset(fixture, 0, sizeof(*fixture));

    if (test_create_temp_dir(fixture->temp_dir, sizeof(fixture->temp_dir)) != 0) {
        return -1;
    }

    if (buffer_size > 0) {
        fixture->test_buffer = (char *)malloc(buffer_size);
        if (fixture->test_buffer == NULL) {
            return -1;
        }
        fixture->test_buffer_size = buffer_size;
    }

    return 0;
}

static inline void test_fixture_teardown(test_fixture_t *fixture)
{
    if (fixture->test_buffer != NULL) {
        free(fixture->test_buffer);
        fixture->test_buffer = NULL;
        fixture->test_buffer_size = 0;
    }
    /* Note: temp_dir cleanup should be done by test or CI system */
}

#endif /* __TEST_HELPERS_H__ */
