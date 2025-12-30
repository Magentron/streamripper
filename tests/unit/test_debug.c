/* test_debug.c - Unit tests for lib/debug.c
 *
 * This test file tests the debug module (lib/debug.c).
 * The debug module provides logging functionality with enable/disable control.
 *
 * Dependencies handled:
 * - sr_strncpy: Simple stub provided below
 * - threadlib_create_sem/waitfor_sem/signal_sem: Stubs provided below
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unity.h>

#include "debug.h"
#include "errors.h"
#include "compat.h"

/* Access to debug module's internal state for testing */
extern int debug_on;
extern FILE *gcsfp;

/*
 * Stub implementations for dependencies
 * These avoid circular dependency issues with mchar.c and threadlib.c
 */

/* Stub for sr_strncpy from mchar.c */
void sr_strncpy(char *dst, char *src, int n)
{
    int i = 0;
    for (i = 0; i < n - 1; i++) {
        if (!(dst[i] = src[i])) {
            return;
        }
    }
    dst[i] = 0;
}

/* Stub implementations for threadlib semaphore functions */
HSEM threadlib_create_sem(void)
{
    HSEM s;
    SemInit(s);
    return s;
}

error_code threadlib_waitfor_sem(HSEM *e)
{
    if (!e) {
        return SR_ERROR_INVALID_PARAM;
    }
    SemWait(*e);
    return SR_SUCCESS;
}

error_code threadlib_signal_sem(HSEM *e)
{
    if (!e) {
        return SR_ERROR_INVALID_PARAM;
    }
    SemPost(*e);
    return SR_SUCCESS;
}

void threadlib_destroy_sem(HSEM *e)
{
    SemDestroy(*e);
}

/* Temp file path for debug output tests */
static char temp_file_path[256];
static int temp_file_created = 0;

/* Helper to reset internal debug state between tests */
static void reset_debug_state(void)
{
    /* Close any open file handle */
    if (gcsfp != NULL) {
        fclose(gcsfp);
        gcsfp = NULL;
    }
    debug_on = 0;
}

void setUp(void)
{
    /* Reset debug state to known values before each test */
    reset_debug_state();

    /* Create a unique temp file path for this test */
    snprintf(temp_file_path, sizeof(temp_file_path),
             "/tmp/test_debug_%d.txt", getpid());
    temp_file_created = 0;
}

void tearDown(void)
{
    /* Clean up debug state */
    reset_debug_state();

    /* Remove temp file if created */
    if (temp_file_created) {
        unlink(temp_file_path);
        temp_file_created = 0;
    }
}

/* Test: debug is disabled by default */
static void test_debug_disabled_by_default(void)
{
    /* debug_on should be 0 after reset */
    TEST_ASSERT_EQUAL(0, debug_on);
}

/* Test: debug_enable sets debug_on to 1 */
static void test_debug_enable_sets_flag(void)
{
    TEST_ASSERT_EQUAL(0, debug_on);

    debug_enable();

    TEST_ASSERT_EQUAL(1, debug_on);
}

/* Test: debug_printf does nothing when disabled */
static void test_debug_printf_disabled_does_nothing(void)
{
    /* Set up a temp file to verify no output */
    debug_set_filename(temp_file_path);
    temp_file_created = 1;

    /* debug_on is 0, so debug_printf should return early */
    TEST_ASSERT_EQUAL(0, debug_on);

    debug_printf("This should not be written\n");

    /* File should not exist since debug is disabled */
    FILE *f = fopen(temp_file_path, "r");
    TEST_ASSERT_NULL(f);
}

/* Test: debug_printf writes when enabled */
static void test_debug_printf_enabled_writes_output(void)
{
    char buffer[1024];

    /* Enable debug and set output file */
    debug_set_filename(temp_file_path);
    temp_file_created = 1;
    debug_enable();

    TEST_ASSERT_EQUAL(1, debug_on);

    /* Write some debug output */
    debug_printf("Test message: %d\n", 42);

    /* Read back the file and verify content */
    FILE *f = fopen(temp_file_path, "r");
    TEST_ASSERT_NOT_NULL(f);

    /* Read entire file */
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[bytes_read] = '\0';
    fclose(f);

    /* File should contain our message (after the header) */
    TEST_ASSERT_NOT_NULL(strstr(buffer, "Test message: 42"));
}

/* Test: debug_open does nothing when disabled */
static void test_debug_open_disabled_does_nothing(void)
{
    debug_set_filename(temp_file_path);
    temp_file_created = 1;

    TEST_ASSERT_EQUAL(0, debug_on);
    TEST_ASSERT_NULL(gcsfp);

    debug_open();

    /* Should still be NULL since debug is disabled */
    TEST_ASSERT_NULL(gcsfp);
}

/* Test: debug_open opens file when enabled */
static void test_debug_open_enabled_opens_file(void)
{
    debug_set_filename(temp_file_path);
    temp_file_created = 1;
    debug_enable();

    TEST_ASSERT_EQUAL(1, debug_on);
    TEST_ASSERT_NULL(gcsfp);

    debug_open();

    /* File pointer should now be set */
    TEST_ASSERT_NOT_NULL(gcsfp);

    /* Clean up - close via reset */
    reset_debug_state();
    TEST_ASSERT_NULL(gcsfp);
}

/* Test: debug_close does nothing when disabled */
static void test_debug_close_disabled_does_nothing(void)
{
    /* Should not crash when debug is disabled */
    TEST_ASSERT_EQUAL(0, debug_on);
    TEST_ASSERT_NULL(gcsfp);

    debug_close();

    /* Should still be null */
    TEST_ASSERT_NULL(gcsfp);
}

/* Test: debug_set_filename sets custom filename */
static void test_debug_set_filename(void)
{
    char buffer[1024];
    const char *custom_path = temp_file_path;

    debug_set_filename((char *)custom_path);
    temp_file_created = 1;
    debug_enable();

    /* Write something to trigger file creation */
    debug_printf("Custom file test\n");

    /* Verify the custom file was created and written to */
    FILE *f = fopen(custom_path, "r");
    TEST_ASSERT_NOT_NULL(f);

    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[bytes_read] = '\0';
    fclose(f);

    TEST_ASSERT_NOT_NULL(strstr(buffer, "Custom file test"));
}

/* Test: debug_printf with format specifiers */
static void test_debug_printf_format_specifiers(void)
{
    char buffer[2048];

    debug_set_filename(temp_file_path);
    temp_file_created = 1;
    debug_enable();

    /* Test various format specifiers */
    debug_printf("String: %s, Int: %d, Hex: 0x%x\n", "hello", 123, 0xABCD);

    /* Read back and verify */
    FILE *f = fopen(temp_file_path, "r");
    TEST_ASSERT_NOT_NULL(f);

    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[bytes_read] = '\0';
    fclose(f);

    TEST_ASSERT_NOT_NULL(strstr(buffer, "String: hello"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "Int: 123"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "Hex: 0xabcd"));
}

/* Test: multiple debug_printf calls append to file */
static void test_debug_printf_appends_output(void)
{
    char buffer[2048];

    debug_set_filename(temp_file_path);
    temp_file_created = 1;
    debug_enable();

    debug_printf("Line 1\n");
    debug_printf("Line 2\n");
    debug_printf("Line 3\n");

    /* Read back and verify all lines are present */
    FILE *f = fopen(temp_file_path, "r");
    TEST_ASSERT_NOT_NULL(f);

    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[bytes_read] = '\0';
    fclose(f);

    TEST_ASSERT_NOT_NULL(strstr(buffer, "Line 1"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "Line 2"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "Line 3"));
}

/* Test: debug_enable followed by disable behavior via debug_on manipulation */
static void test_debug_enable_disable_cycle(void)
{
    char buffer[1024];

    debug_set_filename(temp_file_path);
    temp_file_created = 1;

    /* Enable debug */
    debug_enable();
    TEST_ASSERT_EQUAL(1, debug_on);

    debug_printf("Should appear\n");

    /* Manually disable (simulating what might happen on file open failure) */
    debug_on = 0;

    debug_printf("Should NOT appear\n");

    /* Re-enable */
    debug_on = 1;
    debug_printf("Should also appear\n");

    /* Verify output */
    FILE *f = fopen(temp_file_path, "r");
    TEST_ASSERT_NOT_NULL(f);

    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[bytes_read] = '\0';
    fclose(f);

    TEST_ASSERT_NOT_NULL(strstr(buffer, "Should appear"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "Should also appear"));
    TEST_ASSERT_NULL(strstr(buffer, "Should NOT appear"));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_debug_disabled_by_default);
    RUN_TEST(test_debug_enable_sets_flag);
    RUN_TEST(test_debug_printf_disabled_does_nothing);
    RUN_TEST(test_debug_printf_enabled_writes_output);
    RUN_TEST(test_debug_open_disabled_does_nothing);
    RUN_TEST(test_debug_open_enabled_opens_file);
    RUN_TEST(test_debug_close_disabled_does_nothing);
    RUN_TEST(test_debug_set_filename);
    RUN_TEST(test_debug_printf_format_specifiers);
    RUN_TEST(test_debug_printf_appends_output);
    RUN_TEST(test_debug_enable_disable_cycle);

    return UNITY_END();
}
