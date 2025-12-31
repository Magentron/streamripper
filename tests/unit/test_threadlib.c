/* test_threadlib.c - Unit tests for lib/threadlib.c
 *
 * Tests for threading and semaphore functions.
 *
 * Note: These tests exercise real threading primitives (pthreads/semaphores)
 * rather than using mocks, as the threadlib functions are thin wrappers
 * around OS primitives and need to be tested for actual behavior.
 */
#include <string.h>
#include <unistd.h>
#include <unity.h>

#include "compat.h"
#include "errors.h"
#include "srtypes.h"
#include "threadlib.h"

/* ========================================================================== */
/* Test State Variables                                                       */
/* ========================================================================== */

/* Flag to track if callback was executed */
static volatile int g_callback_executed = 0;

/* Counter to track callback execution count */
static volatile int g_callback_count = 0;

/* Value passed to callback */
static volatile int g_callback_value = 0;

/* Semaphore for synchronizing test thread and callback */
static HSEM g_sync_sem;

/* ========================================================================== */
/* Test Callback Functions                                                    */
/* ========================================================================== */

/* Simple callback that sets a flag and exits */
static void simple_callback(void *arg)
{
    (void)arg;
    g_callback_executed = 1;
}

/* Callback that increments a counter */
static void counter_callback(void *arg)
{
    (void)arg;
    g_callback_count++;
}

/* Callback that stores the passed argument value */
static void value_callback(void *arg)
{
    if (arg != NULL) {
        g_callback_value = *((int *)arg);
    }
    g_callback_executed = 1;
}

/* Callback that waits on a semaphore before completing */
static void sem_wait_callback(void *arg)
{
    HSEM *sem = (HSEM *)arg;
    if (sem != NULL) {
        threadlib_waitfor_sem(sem);
    }
    g_callback_executed = 1;
}

/* Callback that signals a semaphore and then exits */
static void sem_signal_callback(void *arg)
{
    HSEM *sem = (HSEM *)arg;
    g_callback_executed = 1;
    if (sem != NULL) {
        threadlib_signal_sem(sem);
    }
}

/* ========================================================================== */
/* Setup and Teardown                                                         */
/* ========================================================================== */

void setUp(void)
{
    /* Reset test state before each test */
    g_callback_executed = 0;
    g_callback_count = 0;
    g_callback_value = 0;
}

void tearDown(void)
{
    /* Nothing to clean up */
}

/* ========================================================================== */
/* Semaphore Lifecycle Tests                                                  */
/* ========================================================================== */

/* Test: Create and destroy a semaphore */
static void test_sem_create_destroy(void)
{
    HSEM sem = threadlib_create_sem();

    /* Semaphore should be created (non-zero on Unix/POSIX) */
    /* Note: On POSIX systems, sem_t is a struct, so we can't easily
     * test for NULL. We just verify no crash occurs. */

    /* Destroy should complete without error */
    threadlib_destroy_sem(&sem);

    /* Test passed if we got here without crash */
    /* Test passed */
}

/* Test: Create multiple semaphores */
static void test_sem_create_multiple(void)
{
    HSEM sem1 = threadlib_create_sem();
    HSEM sem2 = threadlib_create_sem();
    HSEM sem3 = threadlib_create_sem();

    /* All three should be created successfully */
    /* Destroy in reverse order */
    threadlib_destroy_sem(&sem3);
    threadlib_destroy_sem(&sem2);
    threadlib_destroy_sem(&sem1);

    /* Test passed */
}

/* ========================================================================== */
/* Semaphore Signal/Wait Tests                                                */
/* ========================================================================== */

/* Test: Signal then wait on semaphore */
static void test_sem_signal_then_wait(void)
{
    HSEM sem = threadlib_create_sem();
    error_code result;

    /* Signal the semaphore first */
    result = threadlib_signal_sem(&sem);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Wait should return immediately since semaphore is signaled */
    result = threadlib_waitfor_sem(&sem);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    threadlib_destroy_sem(&sem);
}

/* Test: Multiple signals and waits */
static void test_sem_multiple_signal_wait(void)
{
    HSEM sem = threadlib_create_sem();
    error_code result;

    /* Signal multiple times */
    result = threadlib_signal_sem(&sem);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = threadlib_signal_sem(&sem);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Each wait should succeed */
    result = threadlib_waitfor_sem(&sem);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = threadlib_waitfor_sem(&sem);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    threadlib_destroy_sem(&sem);
}

/* Test: Wait with NULL parameter returns error */
static void test_sem_wait_null_returns_error(void)
{
    error_code result = threadlib_waitfor_sem(NULL);
    TEST_ASSERT_EQUAL(SR_ERROR_INVALID_PARAM, result);
}

/* Test: Signal with NULL parameter returns error */
static void test_sem_signal_null_returns_error(void)
{
    error_code result = threadlib_signal_sem(NULL);
    TEST_ASSERT_EQUAL(SR_ERROR_INVALID_PARAM, result);
}

/* ========================================================================== */
/* Thread Creation Tests                                                      */
/* ========================================================================== */

/* Test: Create a thread with simple callback */
static void test_thread_create_simple(void)
{
    THREAD_HANDLE thread;
    error_code result;

    g_callback_executed = 0;

    result = threadlib_beginthread(&thread, simple_callback, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Wait for thread to complete */
    threadlib_waitforclose(&thread);

    /* Verify callback was executed */
    TEST_ASSERT_EQUAL(1, g_callback_executed);
}

/* Test: Create a thread and pass argument */
static void test_thread_create_with_argument(void)
{
    THREAD_HANDLE thread;
    error_code result;
    int test_value = 42;

    g_callback_executed = 0;
    g_callback_value = 0;

    result = threadlib_beginthread(&thread, value_callback, &test_value);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Wait for thread to complete */
    threadlib_waitforclose(&thread);

    /* Verify callback received the argument */
    TEST_ASSERT_EQUAL(1, g_callback_executed);
    TEST_ASSERT_EQUAL(42, g_callback_value);
}

/* Test: Create multiple threads */
static void test_thread_create_multiple(void)
{
    THREAD_HANDLE thread1, thread2, thread3;
    error_code result;

    g_callback_count = 0;

    result = threadlib_beginthread(&thread1, counter_callback, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = threadlib_beginthread(&thread2, counter_callback, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = threadlib_beginthread(&thread3, counter_callback, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Wait for all threads to complete */
    threadlib_waitforclose(&thread1);
    threadlib_waitforclose(&thread2);
    threadlib_waitforclose(&thread3);

    /* All three callbacks should have executed */
    TEST_ASSERT_EQUAL(3, g_callback_count);
}

/* ========================================================================== */
/* Thread/Semaphore Integration Tests                                         */
/* ========================================================================== */

/* Test: Thread waits on semaphore, main thread signals */
static void test_thread_waits_for_semaphore(void)
{
    THREAD_HANDLE thread;
    HSEM sem;
    error_code result;

    g_callback_executed = 0;
    sem = threadlib_create_sem();

    /* Start thread that waits on semaphore */
    result = threadlib_beginthread(&thread, sem_wait_callback, &sem);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Give thread time to start and begin waiting */
    usleep(10000);  /* 10ms */

    /* Callback should not have completed yet */
    TEST_ASSERT_EQUAL(0, g_callback_executed);

    /* Signal the semaphore to allow thread to complete */
    result = threadlib_signal_sem(&sem);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Wait for thread to complete */
    threadlib_waitforclose(&thread);

    /* Now callback should have completed */
    TEST_ASSERT_EQUAL(1, g_callback_executed);

    threadlib_destroy_sem(&sem);
}

/* Test: Thread signals semaphore, main thread waits */
static void test_thread_signals_semaphore(void)
{
    THREAD_HANDLE thread;
    HSEM sem;
    error_code result;

    g_callback_executed = 0;
    sem = threadlib_create_sem();

    /* Start thread that signals semaphore */
    result = threadlib_beginthread(&thread, sem_signal_callback, &sem);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Wait on semaphore - should unblock when thread signals */
    result = threadlib_waitfor_sem(&sem);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* At this point, thread should have executed */
    TEST_ASSERT_EQUAL(1, g_callback_executed);

    /* Wait for thread to finish */
    threadlib_waitforclose(&thread);

    threadlib_destroy_sem(&sem);
}

/* ========================================================================== */
/* Thread Lifecycle Tests                                                     */
/* ========================================================================== */

/* Test: waitforclose returns after thread completes */
static void test_thread_waitforclose(void)
{
    THREAD_HANDLE thread;
    error_code result;

    g_callback_executed = 0;

    result = threadlib_beginthread(&thread, simple_callback, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* waitforclose should block until thread completes */
    threadlib_waitforclose(&thread);

    /* After waitforclose returns, callback must have executed */
    TEST_ASSERT_EQUAL(1, g_callback_executed);
}

/* Test: Create and wait on thread with NULL argument */
static void test_thread_null_argument(void)
{
    THREAD_HANDLE thread;
    error_code result;

    g_callback_executed = 0;

    result = threadlib_beginthread(&thread, simple_callback, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    threadlib_waitforclose(&thread);
    TEST_ASSERT_EQUAL(1, g_callback_executed);
}

/* ========================================================================== */
/* Stress Tests                                                               */
/* ========================================================================== */

/* Test: Rapid create/destroy of semaphores */
static void test_sem_rapid_create_destroy(void)
{
    int i;
    for (i = 0; i < 100; i++) {
        HSEM sem = threadlib_create_sem();
        threadlib_signal_sem(&sem);
        threadlib_waitfor_sem(&sem);
        threadlib_destroy_sem(&sem);
    }
    /* Test passed */
}

/* Test: Multiple threads with semaphore synchronization */
static void test_multiple_threads_with_sem(void)
{
    THREAD_HANDLE threads[5];
    HSEM sem;
    error_code result;
    int i;

    g_callback_count = 0;
    sem = threadlib_create_sem();

    /* Start all threads - they will wait on semaphore */
    for (i = 0; i < 5; i++) {
        result = threadlib_beginthread(&threads[i], sem_wait_callback, &sem);
        TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    }

    /* Give threads time to start */
    usleep(50000);  /* 50ms */

    /* Signal semaphore 5 times to release all threads */
    for (i = 0; i < 5; i++) {
        result = threadlib_signal_sem(&sem);
        TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    }

    /* Wait for all threads to complete */
    for (i = 0; i < 5; i++) {
        threadlib_waitforclose(&threads[i]);
    }

    threadlib_destroy_sem(&sem);
    /* Test passed */
}

/* ========================================================================== */
/* Main - Run all tests                                                       */
/* ========================================================================== */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* Semaphore lifecycle tests */
    RUN_TEST(test_sem_create_destroy);
    RUN_TEST(test_sem_create_multiple);

    /* Semaphore signal/wait tests */
    RUN_TEST(test_sem_signal_then_wait);
    RUN_TEST(test_sem_multiple_signal_wait);
    RUN_TEST(test_sem_wait_null_returns_error);
    RUN_TEST(test_sem_signal_null_returns_error);

    /* Thread creation tests */
    RUN_TEST(test_thread_create_simple);
    RUN_TEST(test_thread_create_with_argument);
    RUN_TEST(test_thread_create_multiple);

    /* Thread/semaphore integration tests */
    RUN_TEST(test_thread_waits_for_semaphore);
    RUN_TEST(test_thread_signals_semaphore);

    /* Thread lifecycle tests */
    RUN_TEST(test_thread_waitforclose);
    RUN_TEST(test_thread_null_argument);

    /* Stress tests */
    RUN_TEST(test_sem_rapid_create_destroy);
    RUN_TEST(test_multiple_threads_with_sem);

    return UNITY_END();
}
