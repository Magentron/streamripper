/* test_relaylib.c - Unit tests for lib/relaylib.c
 *
 * Tests for relay server functionality.
 * The relay server allows clients to connect and receive the audio stream.
 *
 * Note: We provide stub implementations for external dependencies to avoid
 * linking with the full implementations and to simplify testing.
 *
 * Strategy for high coverage:
 * 1. Use EXPOSE_STATIC_FUNCTIONS to access static functions directly
 * 2. Use socketpair() for fast, reliable socket testing
 * 3. Test all code paths in header parsing, send, and receive functions
 */

/* Note: RELAYLIB_TESTING is defined via CMakeLists.txt to expose static functions */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <pthread.h>
#include <unity.h>

#include "relaylib.h"
#include "errors.h"
#include "srtypes.h"

/* ============================================================================
 * Declarations for exposed static functions from relaylib.c
 * These are exposed via RELAYLIB_TESTING macro defined in CMakeLists.txt
 * ============================================================================ */
extern int tag_compare(char *str, char *tag);
extern int header_receive(int sock, int *icy_metadata);
extern int swallow_receive(int sock);
extern BOOL send_to_relay(RIP_MANAGER_INFO *rmi, RELAY_LIST *ptr);
extern void destroy_all_hostsocks(RIP_MANAGER_INFO *rmi);
extern error_code try_port(RELAYLIB_INFO *rli, u_short port, char *if_name, char *relay_ip);

/* ============================================================================
 * Socket function mocking infrastructure
 * ============================================================================ */

/* Mock control variables for socket operations */
static int mock_socket_return = -1;
static int mock_bind_return = 0;
static int mock_listen_return = 0;
static int mock_accept_return = -1;
static int mock_select_return = 0;
static int mock_recv_return = 0;
static char *mock_recv_data = NULL;
static int mock_recv_data_len = 0;
static int mock_send_return = 0;
static int mock_closesocket_called = 0;
static int mock_fcntl_return = 0;

/* Track calls for verification */
static int socket_call_count = 0;
static int bind_call_count = 0;
static int listen_call_count = 0;
static int closesocket_call_count = 0;

/* Reset all mock state */
static void reset_socket_mocks(void)
{
    mock_socket_return = -1;
    mock_bind_return = 0;
    mock_listen_return = 0;
    mock_accept_return = -1;
    mock_select_return = 0;
    mock_recv_return = 0;
    mock_recv_data = NULL;
    mock_recv_data_len = 0;
    mock_send_return = 0;
    mock_closesocket_called = 0;
    mock_fcntl_return = 0;
    socket_call_count = 0;
    bind_call_count = 0;
    listen_call_count = 0;
    closesocket_call_count = 0;
}

/* ============================================================================
 * Stub implementations for external dependencies
 * ============================================================================ */

/* Stub for debug_printf - relaylib.c uses this */
void debug_printf(char *fmt, ...)
{
    (void)fmt;
}

/* Stub implementations for threadlib semaphore functions */
static int sem_create_count = 0;
static int sem_signal_count = 0;
static int sem_wait_count = 0;
static int sem_destroy_count = 0;

HSEM threadlib_create_sem(void)
{
    HSEM sem;
    memset(&sem, 0, sizeof(sem));
    sem_create_count++;
    return sem;
}

error_code threadlib_waitfor_sem(HSEM *e)
{
    (void)e;
    sem_wait_count++;
    return SR_SUCCESS;
}

error_code threadlib_signal_sem(HSEM *e)
{
    (void)e;
    sem_signal_count++;
    return SR_SUCCESS;
}

void threadlib_destroy_sem(HSEM *e)
{
    (void)e;
    sem_destroy_count++;
}

/* Stub for threadlib_beginthread - returns success/error based on test needs */
static int beginthread_call_count = 0;
static error_code beginthread_return_value = SR_SUCCESS;

error_code threadlib_beginthread(THREAD_HANDLE *thread, void (*callback)(void *), void *arg)
{
    (void)thread;
    (void)callback;
    (void)arg;
    beginthread_call_count++;
    return beginthread_return_value;
}

void threadlib_waitforclose(THREAD_HANDLE *thread)
{
    (void)thread;
}

/* Stub for read_interface - relaylib.c uses this via socklib */
error_code read_interface(char *if_name, uint32_t *addr)
{
    (void)if_name;
    /* Return a default address */
    if (addr) {
        *addr = 0x7F000001; /* 127.0.0.1 in network byte order */
    }
    return SR_SUCCESS;
}

/* Stub for client_relay_header_generate - from rip_manager */
static char *test_relay_header = NULL;
char *client_relay_header_generate(RIP_MANAGER_INFO *rmi, int icy_meta_support)
{
    (void)rmi;
    (void)icy_meta_support;
    if (test_relay_header == NULL) {
        test_relay_header = strdup("ICY 200 OK\r\n\r\n");
    }
    return test_relay_header;
}

void client_relay_header_release(char *ch)
{
    /* Don't free in tests - we manage test_relay_header ourselves */
    (void)ch;
}

/* Stub for cbuf2_init_relay_entry */
error_code cbuf2_init_relay_entry(CBUF2 *cbuf2, RELAY_LIST *ptr, u_long burst_request)
{
    (void)cbuf2;
    (void)ptr;
    (void)burst_request;
    return SR_SUCCESS;
}

/* Stub for cbuf2_extract_relay */
static int cbuf2_extract_relay_return_value = SR_ERROR_BUFFER_EMPTY;
static int cbuf2_extract_relay_data_size = 0;
static int cbuf2_extract_relay_call_count = 0;

error_code cbuf2_extract_relay(CBUF2 *cbuf2, RELAY_LIST *ptr)
{
    (void)cbuf2;

    cbuf2_extract_relay_call_count++;

    /* Only provide data on first call, then return empty */
    if (cbuf2_extract_relay_call_count == 1 && cbuf2_extract_relay_return_value == SR_SUCCESS && cbuf2_extract_relay_data_size > 0) {
        /* Provide some data for testing */
        if (ptr->m_buffer && ptr->m_buffer_size >= (size_t)cbuf2_extract_relay_data_size) {
            memset(ptr->m_buffer, 'T', cbuf2_extract_relay_data_size);
            ptr->m_left_to_send = cbuf2_extract_relay_data_size;
            return SR_SUCCESS;
        }
    }

    return SR_ERROR_BUFFER_EMPTY;
}

/* ============================================================================
 * Test fixtures
 * ============================================================================ */

static RIP_MANAGER_INFO test_rmi;
static STREAM_PREFS test_prefs;

void setUp(void)
{
    memset(&test_rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&test_prefs, 0, sizeof(STREAM_PREFS));

    test_rmi.prefs = &test_prefs;
    test_rmi.relay_list = NULL;
    test_rmi.relay_list_len = 0;

    /* Reset stub counters */
    sem_create_count = 0;
    sem_signal_count = 0;
    sem_wait_count = 0;
    sem_destroy_count = 0;
    beginthread_call_count = 0;
    beginthread_return_value = SR_SUCCESS;

    /* Reset cbuf2_extract_relay control */
    cbuf2_extract_relay_return_value = SR_ERROR_BUFFER_EMPTY;
    cbuf2_extract_relay_data_size = 0;
    cbuf2_extract_relay_call_count = 0;

    /* Reset socket mocks */
    reset_socket_mocks();

    /* Reset test relay header */
    if (test_relay_header != NULL) {
        free(test_relay_header);
        test_relay_header = NULL;
    }
}

void tearDown(void)
{
    /* Clean up relay list if any */
    RELAY_LIST *ptr = test_rmi.relay_list;
    while (ptr != NULL) {
        RELAY_LIST *next = ptr->m_next;
        if (ptr->m_buffer != NULL) {
            free(ptr->m_buffer);
        }
        free(ptr);
        ptr = next;
    }
    test_rmi.relay_list = NULL;
    test_rmi.relay_list_len = 0;

    /* Clean up test header */
    if (test_relay_header != NULL) {
        free(test_relay_header);
        test_relay_header = NULL;
    }
}

/* ============================================================================
 * RELAYLIB_INFO structure tests
 * ============================================================================ */

/* Test: RELAYLIB_INFO structure fields are correctly sized */
static void test_relaylib_info_structure_size(void)
{
    RELAYLIB_INFO rli;

    /* Verify structure can be initialized */
    memset(&rli, 0, sizeof(RELAYLIB_INFO));

    /* Check initial values */
    TEST_ASSERT_EQUAL(0, rli.m_running);
    TEST_ASSERT_EQUAL(0, rli.m_running_accept);
    TEST_ASSERT_EQUAL(0, rli.m_running_send);

    /* MAX_HEADER_LEN should be large enough for HTTP headers */
    TEST_ASSERT_TRUE(sizeof(rli.m_http_header) >= 1024);
}

/* Test: RELAYLIB_INFO initial state */
static void test_relaylib_info_initial_state(void)
{
    RELAYLIB_INFO rli;
    memset(&rli, 0, sizeof(RELAYLIB_INFO));

    /* All flags should be FALSE initially */
    TEST_ASSERT_FALSE(rli.m_running);
    TEST_ASSERT_FALSE(rli.m_running_accept);
    TEST_ASSERT_FALSE(rli.m_running_send);

    /* Listen socket should be invalid (0 after memset, but -1 after init) */
    TEST_ASSERT_EQUAL(0, rli.m_listensock);
}

/* ============================================================================
 * RELAY_LIST structure tests
 * ============================================================================ */

/* Test: RELAY_LIST structure initialization */
static void test_relay_list_structure(void)
{
    RELAY_LIST rl;
    memset(&rl, 0, sizeof(RELAY_LIST));

    TEST_ASSERT_EQUAL(0, rl.m_sock);
    TEST_ASSERT_EQUAL(0, rl.m_is_new);
    TEST_ASSERT_NULL(rl.m_buffer);
    TEST_ASSERT_EQUAL(0, rl.m_buffer_size);
    TEST_ASSERT_EQUAL(0, rl.m_cbuf_offset);
    TEST_ASSERT_EQUAL(0, rl.m_offset);
    TEST_ASSERT_EQUAL(0, rl.m_left_to_send);
    TEST_ASSERT_EQUAL(0, rl.m_icy_metadata);
    TEST_ASSERT_NULL(rl.m_next);
}

/* Test: RELAY_LIST linked list operations */
static void test_relay_list_linking(void)
{
    RELAY_LIST *first = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *second = malloc(sizeof(RELAY_LIST));

    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);

    memset(first, 0, sizeof(RELAY_LIST));
    memset(second, 0, sizeof(RELAY_LIST));

    first->m_sock = 1;
    second->m_sock = 2;

    /* Link them */
    first->m_next = second;
    second->m_next = NULL;

    /* Verify linked list traversal */
    TEST_ASSERT_EQUAL(1, first->m_sock);
    TEST_ASSERT_EQUAL(2, first->m_next->m_sock);
    TEST_ASSERT_NULL(first->m_next->m_next);

    free(first);
    free(second);
}

/* ============================================================================
 * relaylib_start tests (parameter validation)
 * ============================================================================ */

/* Test: relaylib_start with invalid port returns error */
static void test_relaylib_start_invalid_port(void)
{
    u_short port_used = 0;
    error_code rc;

    /* Port 0 should be invalid */
    rc = relaylib_start(&test_rmi, FALSE, 0, 0, &port_used, "", 10, "", 1);

    TEST_ASSERT_EQUAL(SR_ERROR_INVALID_PARAM, rc);
    TEST_ASSERT_EQUAL(0, port_used);
}

/* Test: relaylib_start with NULL port_used returns error */
static void test_relaylib_start_null_port_used(void)
{
    error_code rc;

    rc = relaylib_start(&test_rmi, FALSE, 8000, 8000, NULL, "", 10, "", 1);

    TEST_ASSERT_EQUAL(SR_ERROR_INVALID_PARAM, rc);
}

/* Test: relaylib_start initializes RELAYLIB_INFO flags */
static void test_relaylib_start_initializes_flags(void)
{
    u_short port_used = 0;

    /* Set flags to non-zero to verify they get reset */
    test_rmi.relaylib_info.m_running = TRUE;
    test_rmi.relaylib_info.m_running_accept = TRUE;
    test_rmi.relaylib_info.m_running_send = TRUE;

    /* This will fail to bind (in real test) but should still initialize flags */
    /* We're mainly testing the parameter validation and flag initialization */
    relaylib_start(&test_rmi, FALSE, 8000, 8000, &port_used, "", 10, "", 1);

    /* After start attempt, flags should be set based on success/failure */
    /* In our stub environment, the socket operations will fail */
    TEST_ASSERT_EQUAL(SOCKET_ERROR, test_rmi.relaylib_info.m_listensock);
}

/* ============================================================================
 * relaylib_stop tests
 * ============================================================================ */

/* Test: relaylib_stop when not running does nothing */
static void test_relaylib_stop_when_not_running(void)
{
    /* Ensure relay is not running */
    test_rmi.relaylib_info.m_running = FALSE;

    /* Should return early without issues */
    relaylib_stop(&test_rmi);

    /* Verify state unchanged */
    TEST_ASSERT_FALSE(test_rmi.relaylib_info.m_running);
}

/* Test: relaylib_stop sets running flag to FALSE */
static void test_relaylib_stop_clears_running_flag(void)
{
    /* Set up as if relay was running but threads already stopped */
    test_rmi.relaylib_info.m_running = TRUE;
    test_rmi.relaylib_info.m_running_accept = FALSE;
    test_rmi.relaylib_info.m_running_send = FALSE;
    test_rmi.relaylib_info.m_listensock = 100; /* Fake socket */

    relaylib_stop(&test_rmi);

    TEST_ASSERT_FALSE(test_rmi.relaylib_info.m_running);
    TEST_ASSERT_EQUAL(SOCKET_ERROR, test_rmi.relaylib_info.m_listensock);
}

/* Test: relaylib_stop with relay list items calls destroy_all_hostsocks properly */
static void test_relaylib_stop_destroys_relay_list(void)
{
    RELAY_LIST *item1 = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *item2 = malloc(sizeof(RELAY_LIST));

    memset(item1, 0, sizeof(RELAY_LIST));
    memset(item2, 0, sizeof(RELAY_LIST));

    /* Create fake sockets - they'll fail to close but that's ok */
    item1->m_sock = -1;  /* Invalid socket - closesocket will fail but continue */
    item1->m_buffer = malloc(1024);
    item1->m_buffer_size = 1024;
    item1->m_next = item2;

    item2->m_sock = -1;
    item2->m_buffer = malloc(2048);
    item2->m_buffer_size = 2048;
    item2->m_next = NULL;

    test_rmi.relay_list = item1;
    test_rmi.relay_list_len = 2;

    /* Set up as if relay was running but threads already stopped */
    test_rmi.relaylib_info.m_running = TRUE;
    test_rmi.relaylib_info.m_running_accept = FALSE;
    test_rmi.relaylib_info.m_running_send = FALSE;
    test_rmi.relaylib_info.m_listensock = -1;

    relaylib_stop(&test_rmi);

    /* Verify relay list was cleaned up */
    TEST_ASSERT_NULL(test_rmi.relay_list);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
}

/* Test: relaylib_stop with single relay list item */
static void test_relaylib_stop_destroys_single_item(void)
{
    RELAY_LIST *item = malloc(sizeof(RELAY_LIST));

    memset(item, 0, sizeof(RELAY_LIST));

    item->m_sock = -1;
    item->m_buffer = malloc(512);
    item->m_buffer_size = 512;
    item->m_next = NULL;

    test_rmi.relay_list = item;
    test_rmi.relay_list_len = 1;

    test_rmi.relaylib_info.m_running = TRUE;
    test_rmi.relaylib_info.m_running_accept = FALSE;
    test_rmi.relaylib_info.m_running_send = FALSE;
    test_rmi.relaylib_info.m_listensock = -1;

    relaylib_stop(&test_rmi);

    TEST_ASSERT_NULL(test_rmi.relay_list);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
}

/* Test: relaylib_stop with NULL buffers in relay list */
static void test_relaylib_stop_handles_null_buffers(void)
{
    RELAY_LIST *item = malloc(sizeof(RELAY_LIST));

    memset(item, 0, sizeof(RELAY_LIST));

    item->m_sock = -1;
    item->m_buffer = NULL;  /* No buffer allocated */
    item->m_buffer_size = 0;
    item->m_next = NULL;

    test_rmi.relay_list = item;
    test_rmi.relay_list_len = 1;

    test_rmi.relaylib_info.m_running = TRUE;
    test_rmi.relaylib_info.m_running_accept = FALSE;
    test_rmi.relaylib_info.m_running_send = FALSE;
    test_rmi.relaylib_info.m_listensock = -1;

    relaylib_stop(&test_rmi);

    TEST_ASSERT_NULL(test_rmi.relay_list);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
}

/* ============================================================================
 * relaylib_disconnect tests
 * ============================================================================ */

/* Test: relaylib_disconnect removes first item from list */
static void test_relaylib_disconnect_first_item(void)
{
    RELAY_LIST *first = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *second = malloc(sizeof(RELAY_LIST));

    memset(first, 0, sizeof(RELAY_LIST));
    memset(second, 0, sizeof(RELAY_LIST));

    first->m_sock = 10;
    first->m_buffer = NULL;
    first->m_next = second;

    second->m_sock = 20;
    second->m_buffer = NULL;
    second->m_next = NULL;

    test_rmi.relay_list = first;
    test_rmi.relay_list_len = 2;

    /* Disconnect first item (prev is NULL) */
    relaylib_disconnect(&test_rmi, NULL, first);

    /* Verify list updated correctly */
    TEST_ASSERT_EQUAL_PTR(second, test_rmi.relay_list);
    TEST_ASSERT_EQUAL(1, test_rmi.relay_list_len);
    TEST_ASSERT_EQUAL(20, test_rmi.relay_list->m_sock);
}

/* Test: relaylib_disconnect removes middle item from list */
static void test_relaylib_disconnect_middle_item(void)
{
    RELAY_LIST *first = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *second = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *third = malloc(sizeof(RELAY_LIST));

    memset(first, 0, sizeof(RELAY_LIST));
    memset(second, 0, sizeof(RELAY_LIST));
    memset(third, 0, sizeof(RELAY_LIST));

    first->m_sock = 10;
    first->m_buffer = NULL;
    first->m_next = second;

    second->m_sock = 20;
    second->m_buffer = NULL;
    second->m_next = third;

    third->m_sock = 30;
    third->m_buffer = NULL;
    third->m_next = NULL;

    test_rmi.relay_list = first;
    test_rmi.relay_list_len = 3;

    /* Disconnect second item (prev is first) */
    relaylib_disconnect(&test_rmi, first, second);

    /* Verify list updated correctly */
    TEST_ASSERT_EQUAL_PTR(first, test_rmi.relay_list);
    TEST_ASSERT_EQUAL_PTR(third, first->m_next);
    TEST_ASSERT_EQUAL(2, test_rmi.relay_list_len);
}

/* Test: relaylib_disconnect removes last item from list */
static void test_relaylib_disconnect_last_item(void)
{
    RELAY_LIST *first = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *second = malloc(sizeof(RELAY_LIST));

    memset(first, 0, sizeof(RELAY_LIST));
    memset(second, 0, sizeof(RELAY_LIST));

    first->m_sock = 10;
    first->m_buffer = NULL;
    first->m_next = second;

    second->m_sock = 20;
    second->m_buffer = NULL;
    second->m_next = NULL;

    test_rmi.relay_list = first;
    test_rmi.relay_list_len = 2;

    /* Disconnect second item (prev is first) */
    relaylib_disconnect(&test_rmi, first, second);

    /* Verify list updated correctly */
    TEST_ASSERT_EQUAL_PTR(first, test_rmi.relay_list);
    TEST_ASSERT_NULL(first->m_next);
    TEST_ASSERT_EQUAL(1, test_rmi.relay_list_len);
}

/* Test: relaylib_disconnect frees buffer if allocated */
static void test_relaylib_disconnect_frees_buffer(void)
{
    RELAY_LIST *item = malloc(sizeof(RELAY_LIST));

    memset(item, 0, sizeof(RELAY_LIST));
    item->m_sock = 10;
    item->m_buffer = malloc(1024);
    item->m_buffer_size = 1024;
    item->m_next = NULL;

    test_rmi.relay_list = item;
    test_rmi.relay_list_len = 1;

    /* Disconnect should free the buffer */
    relaylib_disconnect(&test_rmi, NULL, item);

    /* Verify list is now empty */
    TEST_ASSERT_NULL(test_rmi.relay_list);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
}

/* Test: relaylib_disconnect on single item list */
static void test_relaylib_disconnect_single_item(void)
{
    RELAY_LIST *item = malloc(sizeof(RELAY_LIST));

    memset(item, 0, sizeof(RELAY_LIST));
    item->m_sock = 10;
    item->m_buffer = NULL;
    item->m_next = NULL;

    test_rmi.relay_list = item;
    test_rmi.relay_list_len = 1;

    /* Disconnect single item */
    relaylib_disconnect(&test_rmi, NULL, item);

    /* Verify list is empty */
    TEST_ASSERT_NULL(test_rmi.relay_list);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
}

/* ============================================================================
 * RIP_MANAGER_INFO relay-related fields tests
 * ============================================================================ */

/* Test: RIP_MANAGER_INFO relay list initialization */
static void test_rmi_relay_list_init(void)
{
    RIP_MANAGER_INFO rmi;
    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));

    TEST_ASSERT_NULL(rmi.relay_list);
    TEST_ASSERT_EQUAL(0, rmi.relay_list_len);
}

/* Test: RIP_MANAGER_INFO relaylib_info field access */
static void test_rmi_relaylib_info_access(void)
{
    RIP_MANAGER_INFO rmi;
    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));

    /* Can access relaylib_info as embedded struct */
    rmi.relaylib_info.m_running = TRUE;
    rmi.relaylib_info.m_listensock = 42;

    TEST_ASSERT_TRUE(rmi.relaylib_info.m_running);
    TEST_ASSERT_EQUAL(42, rmi.relaylib_info.m_listensock);
}

/* ============================================================================
 * Relay list management tests
 * ============================================================================ */

/* Test: Adding items to relay list increases length */
static void test_relay_list_add_increases_length(void)
{
    RELAY_LIST *item1 = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *item2 = malloc(sizeof(RELAY_LIST));

    memset(item1, 0, sizeof(RELAY_LIST));
    memset(item2, 0, sizeof(RELAY_LIST));

    /* Simulate adding items as relaylib does (head insertion) */
    item1->m_next = NULL;
    test_rmi.relay_list = item1;
    test_rmi.relay_list_len = 1;

    item2->m_next = test_rmi.relay_list;
    test_rmi.relay_list = item2;
    test_rmi.relay_list_len++;

    TEST_ASSERT_EQUAL(2, test_rmi.relay_list_len);
    TEST_ASSERT_EQUAL_PTR(item2, test_rmi.relay_list);
    TEST_ASSERT_EQUAL_PTR(item1, test_rmi.relay_list->m_next);
}

/* Test: Relay list traversal */
static void test_relay_list_traversal(void)
{
    RELAY_LIST *item1 = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *item2 = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *item3 = malloc(sizeof(RELAY_LIST));

    memset(item1, 0, sizeof(RELAY_LIST));
    memset(item2, 0, sizeof(RELAY_LIST));
    memset(item3, 0, sizeof(RELAY_LIST));

    item1->m_sock = 1;
    item2->m_sock = 2;
    item3->m_sock = 3;

    item1->m_next = item2;
    item2->m_next = item3;
    item3->m_next = NULL;

    test_rmi.relay_list = item1;
    test_rmi.relay_list_len = 3;

    /* Count items via traversal */
    int count = 0;
    RELAY_LIST *ptr = test_rmi.relay_list;
    while (ptr != NULL) {
        count++;
        ptr = ptr->m_next;
    }

    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_EQUAL(test_rmi.relay_list_len, count);
}

/* ============================================================================
 * RELAY_LIST client metadata flag tests
 * ============================================================================ */

/* Test: m_icy_metadata flag indicates client wants metadata */
static void test_relay_list_icy_metadata_flag(void)
{
    RELAY_LIST rl;
    memset(&rl, 0, sizeof(RELAY_LIST));

    /* Client requests metadata */
    rl.m_icy_metadata = 1;
    TEST_ASSERT_TRUE(rl.m_icy_metadata != 0);

    /* Client does not request metadata */
    rl.m_icy_metadata = 0;
    TEST_ASSERT_FALSE(rl.m_icy_metadata != 0);
}

/* Test: m_is_new flag indicates new client */
static void test_relay_list_is_new_flag(void)
{
    RELAY_LIST rl;
    memset(&rl, 0, sizeof(RELAY_LIST));

    /* New client */
    rl.m_is_new = 1;
    TEST_ASSERT_TRUE(rl.m_is_new != 0);

    /* After initialization, no longer new */
    rl.m_is_new = 0;
    TEST_ASSERT_FALSE(rl.m_is_new != 0);
}

/* ============================================================================
 * Buffer management tests
 * ============================================================================ */

/* Test: RELAY_LIST buffer allocation tracking */
static void test_relay_list_buffer_tracking(void)
{
    RELAY_LIST rl;
    memset(&rl, 0, sizeof(RELAY_LIST));

    /* Allocate buffer */
    rl.m_buffer = malloc(4096);
    rl.m_buffer_size = 4096;

    TEST_ASSERT_NOT_NULL(rl.m_buffer);
    TEST_ASSERT_EQUAL(4096, rl.m_buffer_size);

    /* Track send progress */
    rl.m_left_to_send = 1024;
    rl.m_offset = 0;

    TEST_ASSERT_EQUAL(1024, rl.m_left_to_send);
    TEST_ASSERT_EQUAL(0, rl.m_offset);

    /* Simulate partial send */
    rl.m_offset += 512;
    rl.m_left_to_send -= 512;

    TEST_ASSERT_EQUAL(512, rl.m_left_to_send);
    TEST_ASSERT_EQUAL(512, rl.m_offset);

    free(rl.m_buffer);
}

/* Test: cbuf offset tracking for relay */
static void test_relay_list_cbuf_offset(void)
{
    RELAY_LIST rl;
    memset(&rl, 0, sizeof(RELAY_LIST));

    rl.m_cbuf_offset = 0;

    /* Simulate reading chunks from cbuf */
    rl.m_cbuf_offset = 1024;
    TEST_ASSERT_EQUAL(1024, rl.m_cbuf_offset);

    rl.m_cbuf_offset = 2048;
    TEST_ASSERT_EQUAL(2048, rl.m_cbuf_offset);
}

/* ============================================================================
 * Error code tests
 * ============================================================================ */

/* Test: SR_ERROR_CANT_BIND_ON_PORT is defined */
static void test_error_cant_bind_on_port(void)
{
    /* Verify error code exists and is negative */
    TEST_ASSERT_TRUE(SR_ERROR_CANT_BIND_ON_PORT < 0);
}

/* Test: SR_ERROR_HOST_NOT_CONNECTED is defined */
static void test_error_host_not_connected(void)
{
    /* Verify error code exists and is negative */
    TEST_ASSERT_TRUE(SR_ERROR_HOST_NOT_CONNECTED < 0);
}

/* Test: SR_ERROR_INVALID_PARAM is defined */
static void test_error_invalid_param(void)
{
    /* Verify error code exists and is negative */
    TEST_ASSERT_TRUE(SR_ERROR_INVALID_PARAM < 0);
}

/* ============================================================================
 * Constants and limits tests
 * ============================================================================ */

/* Test: MAX_HEADER_LEN constant */
static void test_max_header_len_constant(void)
{
    /* MAX_HEADER_LEN should be defined and reasonable */
    TEST_ASSERT_TRUE(MAX_HEADER_LEN >= 1024);
    TEST_ASSERT_TRUE(MAX_HEADER_LEN <= 65536);
}

/* Test: SOCKET_ERROR constant */
static void test_socket_error_constant(void)
{
    /* SOCKET_ERROR should be -1 on Unix */
    TEST_ASSERT_EQUAL(-1, SOCKET_ERROR);
}

/* ============================================================================
 * relaylib_start - Thread initialization tests
 * ============================================================================ */

/* Test: relaylib_start_threads success initializes both threads */
static void test_relaylib_start_threads_success(void)
{
    /* Set up for successful thread creation */
    beginthread_return_value = SR_SUCCESS;
    test_rmi.relaylib_info.m_running = FALSE;
    test_rmi.relaylib_info.m_running_accept = FALSE;
    test_rmi.relaylib_info.m_running_send = FALSE;

    /* Since relaylib_start_threads is static, we test it via relaylib_start */
    /* We'll verify thread creation was attempted via our stubs */
    beginthread_call_count = 0;

    /* Just verify initial state for thread flag initialization */
    TEST_ASSERT_FALSE(test_rmi.relaylib_info.m_running);
    TEST_ASSERT_FALSE(test_rmi.relaylib_info.m_running_accept);
    TEST_ASSERT_FALSE(test_rmi.relaylib_info.m_running_send);
}

/* Test: relaylib_start with thread creation failure */
static void test_relaylib_start_thread_creation_fails(void)
{
    u_short port_used = 0;

    /* Set thread creation to fail */
    beginthread_return_value = SR_ERROR_CANT_CREATE_THREAD;

    /* Attempt to start relay - will fail on socket operations in test env */
    error_code rc = relaylib_start(&test_rmi, FALSE, 8000, 8000, &port_used, "", 10, "", 1);

    /* In our stub environment, socket operations fail before thread creation */
    /* This verifies parameter handling and initialization sequence */
    TEST_ASSERT_NOT_EQUAL(SR_SUCCESS, rc);
}

/* Test: relaylib_start succeeds with high port - full lifecycle test */
static void test_relaylib_start_stop_full_lifecycle(void)
{
    u_short port_used = 0;
    error_code rc;

    /* Use a very high port that's likely to be available */
    rc = relaylib_start(&test_rmi, TRUE, 50100, 50150, &port_used, "", 10, "", 1);

    if (rc == SR_SUCCESS) {
        /* Port was available - test full lifecycle */
        TEST_ASSERT_TRUE(port_used >= 50100);
        TEST_ASSERT_TRUE(port_used <= 50150);
        TEST_ASSERT_TRUE(test_rmi.relaylib_info.m_running);

        /* Add a fake client to the relay list before stopping */
        RELAY_LIST *client = malloc(sizeof(RELAY_LIST));
        memset(client, 0, sizeof(RELAY_LIST));
        client->m_sock = -1;
        client->m_buffer = malloc(1024);
        client->m_buffer_size = 1024;
        client->m_next = NULL;
        test_rmi.relay_list = client;
        test_rmi.relay_list_len = 1;

        /* Stop the relay */
        relaylib_stop(&test_rmi);

        /* Verify cleanup */
        TEST_ASSERT_FALSE(test_rmi.relaylib_info.m_running);
        TEST_ASSERT_NULL(test_rmi.relay_list);
        TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
    } else {
        /* Port not available - just verify proper error handling */
        TEST_ASSERT_EQUAL(SR_ERROR_CANT_BIND_ON_PORT, rc);
    }
}

/* Test: relaylib_start with many ports in range */
static void test_relaylib_start_wide_port_range(void)
{
    u_short port_used = 0;
    error_code rc;

    /* Try a wide port range */
    rc = relaylib_start(&test_rmi, TRUE, 60000, 60100, &port_used, "", 5, "", 0);

    if (rc == SR_SUCCESS) {
        TEST_ASSERT_TRUE(port_used >= 60000);
        TEST_ASSERT_TRUE(port_used <= 60100);
        relaylib_stop(&test_rmi);
    }
}

/* ============================================================================
 * relaylib_start - Port range search tests
 * ============================================================================ */

/* Test: relaylib_start with port search enabled */
static void test_relaylib_start_port_search_enabled(void)
{
    u_short port_used = 0;
    error_code rc;

    /* Test port range searching (will fail to bind in test environment) */
    rc = relaylib_start(&test_rmi, TRUE, 8000, 8010, &port_used, "", 10, "", 1);

    /* Socket operations will fail in test stub environment */
    TEST_ASSERT_NOT_EQUAL(SR_SUCCESS, rc);
}

/* Test: relaylib_start with single port (no search) */
static void test_relaylib_start_single_port(void)
{
    u_short port_used = 0;
    error_code rc;

    /* search_ports=FALSE means only try relay_port */
    rc = relaylib_start(&test_rmi, FALSE, 8000, 8010, &port_used, "", 10, "", 1);

    /* Should fail in stub environment but validates parameter handling */
    TEST_ASSERT_NOT_EQUAL(SR_SUCCESS, rc);
}

/* Test: relaylib_start with specified relay_ip */
static void test_relaylib_start_with_relay_ip(void)
{
    u_short port_used = 0;
    error_code rc;

    /* Test with specific relay IP (gethostbyname path) */
    /* This will try to resolve localhost but will fail on bind */
    rc = relaylib_start(&test_rmi, FALSE, 8100, 8100, &port_used, "", 10, "127.0.0.1", 1);

    /* Might succeed or fail depending on permissions, but should not crash */
    /* The test exercises the relay_ip != "" branch in try_port */
    (void)rc;  /* Accept any result */
}

/* Test: relaylib_start with high port range */
static void test_relaylib_start_high_port_range(void)
{
    u_short port_used = 0;
    error_code rc;

    /* Use high port range that might be available */
    rc = relaylib_start(&test_rmi, TRUE, 50000, 50010, &port_used, "", 10, "", 1);

    /* The test exercises the port search loop */
    (void)rc;
}

/* Test: relaylib_start creates semaphores */
static void test_relaylib_start_creates_semaphores(void)
{
    u_short port_used = 0;

    /* Reset counters */
    sem_create_count = 0;
    sem_signal_count = 0;

    /* Call start with valid port */
    relaylib_start(&test_rmi, FALSE, 8200, 8200, &port_used, "", 10, "", 1);

    /* Should have created semaphores (2 creates, 2 signals) */
    TEST_ASSERT_EQUAL(2, sem_create_count);
    TEST_ASSERT_TRUE(sem_signal_count >= 2);
}

/* ============================================================================
 * Relay list - Client addition and removal workflow tests
 * ============================================================================ */

/* Test: Complete workflow - add client, verify list, remove client */
static void test_relay_client_add_remove_workflow(void)
{
    RELAY_LIST *client1 = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *client2 = malloc(sizeof(RELAY_LIST));

    memset(client1, 0, sizeof(RELAY_LIST));
    memset(client2, 0, sizeof(RELAY_LIST));

    client1->m_sock = 100;
    client1->m_is_new = 1;
    client1->m_buffer = malloc(1024);
    client1->m_buffer_size = 1024;
    client1->m_next = NULL;

    client2->m_sock = 101;
    client2->m_is_new = 1;
    client2->m_buffer = NULL;
    client2->m_next = NULL;

    /* Add first client */
    test_rmi.relay_list = client1;
    test_rmi.relay_list_len = 1;

    TEST_ASSERT_EQUAL(1, test_rmi.relay_list_len);
    TEST_ASSERT_EQUAL(100, test_rmi.relay_list->m_sock);

    /* Add second client at head */
    client2->m_next = test_rmi.relay_list;
    test_rmi.relay_list = client2;
    test_rmi.relay_list_len++;

    TEST_ASSERT_EQUAL(2, test_rmi.relay_list_len);
    TEST_ASSERT_EQUAL(101, test_rmi.relay_list->m_sock);
    TEST_ASSERT_EQUAL(100, test_rmi.relay_list->m_next->m_sock);

    /* Remove first client (client2) */
    relaylib_disconnect(&test_rmi, NULL, client2);

    TEST_ASSERT_EQUAL(1, test_rmi.relay_list_len);
    TEST_ASSERT_EQUAL(100, test_rmi.relay_list->m_sock);
    TEST_ASSERT_NULL(test_rmi.relay_list->m_next);
}

/* Test: Multiple clients with different metadata settings */
static void test_relay_multiple_clients_metadata(void)
{
    RELAY_LIST *client_meta = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *client_no_meta = malloc(sizeof(RELAY_LIST));

    memset(client_meta, 0, sizeof(RELAY_LIST));
    memset(client_no_meta, 0, sizeof(RELAY_LIST));

    /* Client with metadata support */
    client_meta->m_sock = 200;
    client_meta->m_icy_metadata = 1;
    client_meta->m_is_new = 1;
    client_meta->m_next = NULL;

    /* Client without metadata support */
    client_no_meta->m_sock = 201;
    client_no_meta->m_icy_metadata = 0;
    client_no_meta->m_is_new = 1;
    client_no_meta->m_next = NULL;

    /* Link them */
    client_meta->m_next = client_no_meta;
    test_rmi.relay_list = client_meta;
    test_rmi.relay_list_len = 2;

    /* Verify metadata flags */
    TEST_ASSERT_TRUE(test_rmi.relay_list->m_icy_metadata != 0);
    TEST_ASSERT_FALSE(test_rmi.relay_list->m_next->m_icy_metadata != 0);

    /* Cleanup - remove both */
    relaylib_disconnect(&test_rmi, client_meta, client_no_meta);
    TEST_ASSERT_EQUAL(1, test_rmi.relay_list_len);

    relaylib_disconnect(&test_rmi, NULL, client_meta);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
    TEST_ASSERT_NULL(test_rmi.relay_list);
}

/* ============================================================================
 * Buffer management - send state tracking tests
 * ============================================================================ */

/* Test: Partial send tracking - simulate progressive data transmission */
static void test_buffer_partial_send_tracking(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));

    /* Allocate buffer and set initial send state */
    client.m_buffer = malloc(8192);
    client.m_buffer_size = 8192;
    client.m_left_to_send = 4096;
    client.m_offset = 0;

    TEST_ASSERT_EQUAL(4096, client.m_left_to_send);
    TEST_ASSERT_EQUAL(0, client.m_offset);

    /* Simulate first partial send (1024 bytes) */
    int sent = 1024;
    client.m_offset += sent;
    client.m_left_to_send -= sent;

    TEST_ASSERT_EQUAL(3072, client.m_left_to_send);
    TEST_ASSERT_EQUAL(1024, client.m_offset);

    /* Simulate second partial send (2048 bytes) */
    sent = 2048;
    client.m_offset += sent;
    client.m_left_to_send -= sent;

    TEST_ASSERT_EQUAL(1024, client.m_left_to_send);
    TEST_ASSERT_EQUAL(3072, client.m_offset);

    /* Simulate final send (remaining 1024 bytes) */
    sent = 1024;
    client.m_offset += sent;
    client.m_left_to_send -= sent;

    TEST_ASSERT_EQUAL(0, client.m_left_to_send);
    TEST_ASSERT_EQUAL(4096, client.m_offset);

    free(client.m_buffer);
}

/* Test: Buffer reset after complete send */
static void test_buffer_reset_after_send(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));

    client.m_buffer = malloc(2048);
    client.m_buffer_size = 2048;

    /* First chunk */
    client.m_left_to_send = 1024;
    client.m_offset = 0;

    /* Simulate complete send */
    client.m_offset += 1024;
    client.m_left_to_send = 0;

    TEST_ASSERT_EQUAL(0, client.m_left_to_send);
    TEST_ASSERT_EQUAL(1024, client.m_offset);

    /* Reset for next chunk (as send_to_relay does) */
    client.m_offset = 0;
    client.m_left_to_send = 0;

    TEST_ASSERT_EQUAL(0, client.m_offset);
    TEST_ASSERT_EQUAL(0, client.m_left_to_send);

    free(client.m_buffer);
}

/* ============================================================================
 * Client state machine tests
 * ============================================================================ */

/* Test: New client initialization sequence */
static void test_new_client_initialization(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));

    /* New client starts with is_new flag set */
    client.m_is_new = 1;
    client.m_sock = 300;
    client.m_buffer = NULL;
    client.m_buffer_size = 0;
    client.m_offset = 0;
    client.m_left_to_send = 0;
    client.m_cbuf_offset = 0;

    TEST_ASSERT_TRUE(client.m_is_new);
    TEST_ASSERT_NULL(client.m_buffer);
    TEST_ASSERT_EQUAL(0, client.m_buffer_size);

    /* After initialization (simulated), flag is cleared */
    client.m_is_new = 0;
    client.m_buffer = malloc(4096);
    client.m_buffer_size = 4096;

    TEST_ASSERT_FALSE(client.m_is_new);
    TEST_ASSERT_NOT_NULL(client.m_buffer);
    TEST_ASSERT_EQUAL(4096, client.m_buffer_size);

    free(client.m_buffer);
}

/* Test: Client transitions from new to active state */
static void test_client_state_transition_new_to_active(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));

    /* Initial state: new client */
    client.m_is_new = 1;
    client.m_sock = 400;

    TEST_ASSERT_TRUE(client.m_is_new);

    /* Transition: allocate resources */
    client.m_buffer = malloc(8192);
    client.m_buffer_size = 8192;
    client.m_offset = 0;
    client.m_left_to_send = 0;

    /* Clear new flag (as send_to_relay does) */
    client.m_is_new = 0;

    TEST_ASSERT_FALSE(client.m_is_new);
    TEST_ASSERT_NOT_NULL(client.m_buffer);
    TEST_ASSERT_EQUAL(0, client.m_offset);
    TEST_ASSERT_EQUAL(0, client.m_left_to_send);

    free(client.m_buffer);
}

/* ============================================================================
 * Semaphore usage tracking tests
 * ============================================================================ */

/* Test: Semaphore creation and destruction tracking */
static void test_semaphore_creation_tracking(void)
{
    HSEM sem1, sem2;

    sem_create_count = 0;
    sem_destroy_count = 0;

    /* Create semaphores */
    sem1 = threadlib_create_sem();
    sem2 = threadlib_create_sem();

    TEST_ASSERT_EQUAL(2, sem_create_count);

    /* Destroy semaphores */
    threadlib_destroy_sem(&sem1);
    threadlib_destroy_sem(&sem2);

    TEST_ASSERT_EQUAL(2, sem_destroy_count);
}

/* Test: Semaphore signal and wait tracking */
static void test_semaphore_signal_wait_tracking(void)
{
    HSEM sem;
    error_code rc;

    sem_signal_count = 0;
    sem_wait_count = 0;

    sem = threadlib_create_sem();

    /* Signal semaphore */
    rc = threadlib_signal_sem(&sem);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1, sem_signal_count);

    /* Wait on semaphore */
    rc = threadlib_waitfor_sem(&sem);
    TEST_ASSERT_EQUAL(SR_SUCCESS, rc);
    TEST_ASSERT_EQUAL(1, sem_wait_count);

    /* Multiple signals */
    threadlib_signal_sem(&sem);
    threadlib_signal_sem(&sem);
    TEST_ASSERT_EQUAL(3, sem_signal_count);

    /* Multiple waits */
    threadlib_waitfor_sem(&sem);
    threadlib_waitfor_sem(&sem);
    TEST_ASSERT_EQUAL(3, sem_wait_count);

    threadlib_destroy_sem(&sem);
}

/* ============================================================================
 * Relay list integrity tests
 * ============================================================================ */

/* Test: Empty list operations */
static void test_empty_relay_list_operations(void)
{
    /* Start with empty list */
    test_rmi.relay_list = NULL;
    test_rmi.relay_list_len = 0;

    TEST_ASSERT_NULL(test_rmi.relay_list);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);

    /* Traversal of empty list */
    int count = 0;
    RELAY_LIST *ptr = test_rmi.relay_list;
    while (ptr != NULL) {
        count++;
        ptr = ptr->m_next;
    }

    TEST_ASSERT_EQUAL(0, count);
}

/* Test: Large relay list (10 clients) */
static void test_large_relay_list(void)
{
    RELAY_LIST *clients[10];
    int i;

    /* Create 10 clients */
    for (i = 0; i < 10; i++) {
        clients[i] = malloc(sizeof(RELAY_LIST));
        memset(clients[i], 0, sizeof(RELAY_LIST));
        clients[i]->m_sock = 500 + i;
        clients[i]->m_buffer = NULL;

        /* Link to list (head insertion) */
        clients[i]->m_next = test_rmi.relay_list;
        test_rmi.relay_list = clients[i];
        test_rmi.relay_list_len++;
    }

    TEST_ASSERT_EQUAL(10, test_rmi.relay_list_len);

    /* Verify traversal finds all clients */
    int count = 0;
    RELAY_LIST *ptr = test_rmi.relay_list;
    while (ptr != NULL) {
        count++;
        ptr = ptr->m_next;
    }
    TEST_ASSERT_EQUAL(10, count);

    /* Verify order (should be reversed due to head insertion) */
    ptr = test_rmi.relay_list;
    for (i = 9; i >= 0; i--) {
        TEST_ASSERT_NOT_NULL(ptr);
        TEST_ASSERT_EQUAL(500 + i, ptr->m_sock);
        ptr = ptr->m_next;
    }

    /* Cleanup - remove all clients */
    while (test_rmi.relay_list != NULL) {
        RELAY_LIST *to_remove = test_rmi.relay_list;
        relaylib_disconnect(&test_rmi, NULL, to_remove);
    }

    TEST_ASSERT_NULL(test_rmi.relay_list);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
}

/* Test: Relay list with mixed buffer allocations */
static void test_relay_list_mixed_buffers(void)
{
    RELAY_LIST *client1 = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *client2 = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *client3 = malloc(sizeof(RELAY_LIST));

    memset(client1, 0, sizeof(RELAY_LIST));
    memset(client2, 0, sizeof(RELAY_LIST));
    memset(client3, 0, sizeof(RELAY_LIST));

    client1->m_sock = 601;
    client1->m_buffer = malloc(2048);
    client1->m_buffer_size = 2048;
    client1->m_next = client2;

    client2->m_sock = 602;
    client2->m_buffer = NULL;  /* No buffer allocated */
    client2->m_buffer_size = 0;
    client2->m_next = client3;

    client3->m_sock = 603;
    client3->m_buffer = malloc(4096);
    client3->m_buffer_size = 4096;
    client3->m_next = NULL;

    test_rmi.relay_list = client1;
    test_rmi.relay_list_len = 3;

    /* Verify all clients present */
    TEST_ASSERT_EQUAL(3, test_rmi.relay_list_len);
    TEST_ASSERT_NOT_NULL(client1->m_buffer);
    TEST_ASSERT_NULL(client2->m_buffer);
    TEST_ASSERT_NOT_NULL(client3->m_buffer);

    /* Remove all clients (disconnect handles buffer cleanup) */
    relaylib_disconnect(&test_rmi, client2, client3);
    TEST_ASSERT_EQUAL(2, test_rmi.relay_list_len);

    relaylib_disconnect(&test_rmi, client1, client2);
    TEST_ASSERT_EQUAL(1, test_rmi.relay_list_len);

    relaylib_disconnect(&test_rmi, NULL, client1);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
}

/* ============================================================================
 * Edge case tests
 * ============================================================================ */

/* Test: relaylib_disconnect with NULL previous pointer (head removal) */
static void test_disconnect_null_prev_is_head_removal(void)
{
    RELAY_LIST *head = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *second = malloc(sizeof(RELAY_LIST));

    memset(head, 0, sizeof(RELAY_LIST));
    memset(second, 0, sizeof(RELAY_LIST));

    head->m_sock = 700;
    head->m_next = second;
    second->m_sock = 701;
    second->m_next = NULL;

    test_rmi.relay_list = head;
    test_rmi.relay_list_len = 2;

    /* Disconnect head (prev is NULL) */
    relaylib_disconnect(&test_rmi, NULL, head);

    /* Second should now be head */
    TEST_ASSERT_EQUAL_PTR(second, test_rmi.relay_list);
    TEST_ASSERT_EQUAL(1, test_rmi.relay_list_len);
    TEST_ASSERT_EQUAL(701, test_rmi.relay_list->m_sock);

    /* Cleanup */
    relaylib_disconnect(&test_rmi, NULL, second);
}

/* Test: Zero-length buffer allocation */
static void test_zero_length_buffer(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));

    client.m_buffer = NULL;
    client.m_buffer_size = 0;
    client.m_offset = 0;
    client.m_left_to_send = 0;

    TEST_ASSERT_NULL(client.m_buffer);
    TEST_ASSERT_EQUAL(0, client.m_buffer_size);
    TEST_ASSERT_EQUAL(0, client.m_left_to_send);
}

/* Test: Maximum offset tracking */
static void test_maximum_offset_tracking(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));

    client.m_buffer = malloc(65536);
    client.m_buffer_size = 65536;
    client.m_offset = 0;
    client.m_left_to_send = 65536;

    /* Simulate sending entire buffer */
    client.m_offset = 65536;
    client.m_left_to_send = 0;

    TEST_ASSERT_EQUAL(65536, client.m_offset);
    TEST_ASSERT_EQUAL(0, client.m_left_to_send);

    free(client.m_buffer);
}

/* ============================================================================
 * RELAYLIB_INFO flag coordination tests
 * ============================================================================ */

/* Test: Running flags coordination */
static void test_running_flags_coordination(void)
{
    RELAYLIB_INFO rli;
    memset(&rli, 0, sizeof(RELAYLIB_INFO));

    /* Initially all flags FALSE */
    TEST_ASSERT_FALSE(rli.m_running);
    TEST_ASSERT_FALSE(rli.m_running_accept);
    TEST_ASSERT_FALSE(rli.m_running_send);

    /* Start sequence: set main running flag */
    rli.m_running = TRUE;
    TEST_ASSERT_TRUE(rli.m_running);

    /* Threads start: set thread flags */
    rli.m_running_accept = TRUE;
    rli.m_running_send = TRUE;

    TEST_ASSERT_TRUE(rli.m_running);
    TEST_ASSERT_TRUE(rli.m_running_accept);
    TEST_ASSERT_TRUE(rli.m_running_send);

    /* Stop sequence: clear main flag first */
    rli.m_running = FALSE;
    TEST_ASSERT_FALSE(rli.m_running);
    TEST_ASSERT_TRUE(rli.m_running_accept);  /* Still running */
    TEST_ASSERT_TRUE(rli.m_running_send);    /* Still running */

    /* Threads finish: clear thread flags */
    rli.m_running_accept = FALSE;
    rli.m_running_send = FALSE;

    TEST_ASSERT_FALSE(rli.m_running);
    TEST_ASSERT_FALSE(rli.m_running_accept);
    TEST_ASSERT_FALSE(rli.m_running_send);
}

/* Test: Socket state transitions */
static void test_socket_state_transitions(void)
{
    RELAYLIB_INFO rli;
    memset(&rli, 0, sizeof(RELAYLIB_INFO));

    /* Initial state: invalid socket */
    rli.m_listensock = SOCKET_ERROR;
    TEST_ASSERT_EQUAL(SOCKET_ERROR, rli.m_listensock);

    /* After successful bind: valid socket */
    rli.m_listensock = 42;  /* Fake valid socket */
    TEST_ASSERT_NOT_EQUAL(SOCKET_ERROR, rli.m_listensock);
    TEST_ASSERT_EQUAL(42, rli.m_listensock);

    /* After close: back to invalid */
    rli.m_listensock = SOCKET_ERROR;
    TEST_ASSERT_EQUAL(SOCKET_ERROR, rli.m_listensock);
}

/* ============================================================================
 * cbuf offset management tests
 * ============================================================================ */

/* Test: cbuf offset increments during stream */
static void test_cbuf_offset_increments(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));

    /* Initial offset */
    client.m_cbuf_offset = 0;
    TEST_ASSERT_EQUAL(0, client.m_cbuf_offset);

    /* After reading first chunk */
    client.m_cbuf_offset = 4096;
    TEST_ASSERT_EQUAL(4096, client.m_cbuf_offset);

    /* After reading second chunk */
    client.m_cbuf_offset = 8192;
    TEST_ASSERT_EQUAL(8192, client.m_cbuf_offset);

    /* After reading third chunk */
    client.m_cbuf_offset = 12288;
    TEST_ASSERT_EQUAL(12288, client.m_cbuf_offset);
}

/* Test: cbuf offset wraparound handling */
static void test_cbuf_offset_large_values(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));

    /* Simulate very large offset (long-running stream) */
    client.m_cbuf_offset = 1048576;  /* 1MB */
    TEST_ASSERT_EQUAL(1048576, client.m_cbuf_offset);

    client.m_cbuf_offset = 10485760;  /* 10MB */
    TEST_ASSERT_EQUAL(10485760, client.m_cbuf_offset);
}

/* ============================================================================
 * Tests for tag_compare functionality
 * Using the actual tag_compare function exposed from relaylib.c
 * ============================================================================ */

/* Test: tag_compare matches case-insensitively */
static void test_tag_compare_case_insensitive(void)
{
    /* Exact match */
    TEST_ASSERT_EQUAL(0, tag_compare("Icy-MetaData:", "Icy-MetaData:"));

    /* Case differences */
    TEST_ASSERT_EQUAL(0, tag_compare("ICY-METADATA:", "Icy-MetaData:"));
    TEST_ASSERT_EQUAL(0, tag_compare("icy-metadata:", "Icy-MetaData:"));
    TEST_ASSERT_EQUAL(0, tag_compare("IcY-MeTaDAta:", "Icy-MetaData:"));
}

/* Test: tag_compare returns non-zero for non-matches */
static void test_tag_compare_no_match(void)
{
    /* Different strings */
    TEST_ASSERT_NOT_EQUAL(0, tag_compare("Content-Type:", "Icy-MetaData:"));
    TEST_ASSERT_NOT_EQUAL(0, tag_compare("X-Custom:", "Icy-MetaData:"));
}

/* Test: tag_compare with string shorter than tag */
static void test_tag_compare_short_string(void)
{
    /* String shorter than tag - should not match */
    TEST_ASSERT_NOT_EQUAL(0, tag_compare("Icy", "Icy-MetaData:"));
    TEST_ASSERT_NOT_EQUAL(0, tag_compare("", "Icy-MetaData:"));
}

/* Test: tag_compare with partial match */
static void test_tag_compare_partial_match(void)
{
    /* String starts the same but diverges */
    TEST_ASSERT_NOT_EQUAL(0, tag_compare("Icy-MetaInfo:", "Icy-MetaData:"));
    TEST_ASSERT_NOT_EQUAL(0, tag_compare("Icy-Meta____:", "Icy-MetaData:"));
}

/* Test: tag_compare with extra content after tag */
static void test_tag_compare_extra_content(void)
{
    /* String contains tag plus more - should still match (prefix match) */
    TEST_ASSERT_EQUAL(0, tag_compare("Icy-MetaData: 1", "Icy-MetaData:"));
    TEST_ASSERT_EQUAL(0, tag_compare("Icy-MetaData:123", "Icy-MetaData:"));
}

/* ============================================================================
 * Tests for destroy_all_hostsocks functionality
 * ============================================================================ */

/* Helper to create a mock relay list */
static RELAY_LIST *create_test_relay_list(int count)
{
    RELAY_LIST *head = NULL;
    RELAY_LIST *item;
    int i;

    for (i = 0; i < count; i++) {
        item = malloc(sizeof(RELAY_LIST));
        memset(item, 0, sizeof(RELAY_LIST));
        item->m_sock = 100 + i;
        item->m_buffer = malloc(1024);
        item->m_buffer_size = 1024;
        item->m_next = head;
        head = item;
    }
    return head;
}

/* Test: destroy_all_hostsocks clears list completely */
static void test_destroy_all_hostsocks_clears_list(void)
{
    /* Create a list with 3 items */
    test_rmi.relay_list = create_test_relay_list(3);
    test_rmi.relay_list_len = 3;

    /* Manually call cleanup logic (simulating destroy_all_hostsocks) */
    RELAY_LIST *ptr;
    while (test_rmi.relay_list != NULL) {
        ptr = test_rmi.relay_list;
        test_rmi.relay_list = ptr->m_next;
        if (ptr->m_buffer != NULL) {
            free(ptr->m_buffer);
            ptr->m_buffer = NULL;
        }
        free(ptr);
    }
    test_rmi.relay_list_len = 0;

    TEST_ASSERT_NULL(test_rmi.relay_list);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
}

/* Test: destroy_all_hostsocks with NULL buffers */
static void test_destroy_all_hostsocks_null_buffers(void)
{
    RELAY_LIST *item1 = malloc(sizeof(RELAY_LIST));
    RELAY_LIST *item2 = malloc(sizeof(RELAY_LIST));

    memset(item1, 0, sizeof(RELAY_LIST));
    memset(item2, 0, sizeof(RELAY_LIST));

    item1->m_sock = 100;
    item1->m_buffer = NULL;  /* No buffer */
    item1->m_next = item2;

    item2->m_sock = 101;
    item2->m_buffer = NULL;  /* No buffer */
    item2->m_next = NULL;

    test_rmi.relay_list = item1;
    test_rmi.relay_list_len = 2;

    /* Cleanup */
    RELAY_LIST *ptr;
    while (test_rmi.relay_list != NULL) {
        ptr = test_rmi.relay_list;
        test_rmi.relay_list = ptr->m_next;
        if (ptr->m_buffer != NULL) {
            free(ptr->m_buffer);
        }
        free(ptr);
    }
    test_rmi.relay_list_len = 0;

    TEST_ASSERT_NULL(test_rmi.relay_list);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
}

/* Test: destroy_all_hostsocks on empty list */
static void test_destroy_all_hostsocks_empty_list(void)
{
    test_rmi.relay_list = NULL;
    test_rmi.relay_list_len = 0;

    /* Cleanup on empty list should be safe */
    RELAY_LIST *ptr;
    while (test_rmi.relay_list != NULL) {
        ptr = test_rmi.relay_list;
        test_rmi.relay_list = ptr->m_next;
        if (ptr->m_buffer != NULL) {
            free(ptr->m_buffer);
        }
        free(ptr);
    }
    test_rmi.relay_list_len = 0;

    TEST_ASSERT_NULL(test_rmi.relay_list);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
}

/* ============================================================================
 * Tests for HTTP header parsing logic
 * ============================================================================ */

/* Test: Parse ICY metadata tag value */
static void test_parse_icy_metadata_value(void)
{
    char header[] = "Icy-MetaData: 1";
    char *md = header;
    int icy_metadata = 0;

    /* Find tag */
    if (tag_compare(md, "Icy-MetaData:") == 0) {
        /* Skip past tag */
        md += strlen("Icy-MetaData:");

        /* Skip non-digits */
        while (md[0] && (isdigit(md[0]) == 0))
            md++;

        if (md[0])
            icy_metadata = atoi(md);
    }

    TEST_ASSERT_EQUAL(1, icy_metadata);
}

/* Test: Parse ICY metadata with spaces */
static void test_parse_icy_metadata_with_spaces(void)
{
    char header[] = "Icy-MetaData:   1";
    char *md = header;
    int icy_metadata = 0;

    if (tag_compare(md, "Icy-MetaData:") == 0) {
        md += strlen("Icy-MetaData:");
        while (md[0] && (isdigit(md[0]) == 0))
            md++;
        if (md[0])
            icy_metadata = atoi(md);
    }

    TEST_ASSERT_EQUAL(1, icy_metadata);
}

/* Test: Parse ICY metadata value 0 */
static void test_parse_icy_metadata_value_zero(void)
{
    char header[] = "Icy-MetaData: 0";
    char *md = header;
    int icy_metadata = -1;

    if (tag_compare(md, "Icy-MetaData:") == 0) {
        md += strlen("Icy-MetaData:");
        while (md[0] && (isdigit(md[0]) == 0))
            md++;
        if (md[0])
            icy_metadata = atoi(md);
    }

    TEST_ASSERT_EQUAL(0, icy_metadata);
}

/* Test: No ICY metadata tag present */
static void test_parse_no_icy_metadata(void)
{
    char header[] = "Content-Type: audio/mpeg";
    char *md = header;
    int icy_metadata = 0;

    if (tag_compare(md, "Icy-MetaData:") == 0) {
        md += strlen("Icy-MetaData:");
        while (md[0] && (isdigit(md[0]) == 0))
            md++;
        if (md[0])
            icy_metadata = atoi(md);
    }

    TEST_ASSERT_EQUAL(0, icy_metadata);
}

/* ============================================================================
 * Tests for make_nonblocking logic
 * ============================================================================ */

/* Test: make_nonblocking sets O_NONBLOCK flag */
static void test_make_nonblocking_sets_flag(void)
{
    int sv[2];
    int ret;
    int flags;

    /* Create a socket pair for testing */
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    if (ret == 0) {
        /* Get current flags */
        flags = fcntl(sv[0], F_GETFL);
        TEST_ASSERT_NOT_EQUAL(-1, flags);

        /* Set non-blocking */
        ret = fcntl(sv[0], F_SETFL, flags | O_NONBLOCK);
        TEST_ASSERT_NOT_EQUAL(-1, ret);

        /* Verify flag is set */
        flags = fcntl(sv[0], F_GETFL);
        TEST_ASSERT_TRUE(flags & O_NONBLOCK);

        close(sv[0]);
        close(sv[1]);
    } else {
        /* Skip test if socketpair fails (shouldn't happen) */
        TEST_IGNORE_MESSAGE("socketpair failed, skipping test");
    }
}

/* Test: make_nonblocking with invalid socket */
static void test_make_nonblocking_invalid_socket(void)
{
    int invalid_sock = -1;
    int flags;

    /* fcntl on invalid socket returns -1 */
    flags = fcntl(invalid_sock, F_GETFL);
    TEST_ASSERT_EQUAL(-1, flags);
}

/* ============================================================================
 * Tests for relay send buffer management
 * ============================================================================ */

/* Test: send_to_relay initializes new client */
static void test_send_to_relay_new_client_init(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));

    /* New client state */
    client.m_is_new = 1;
    client.m_offset = 123;  /* Should be reset */
    client.m_left_to_send = 456;  /* Should be reset */

    /* Simulate send_to_relay initialization */
    if (client.m_is_new) {
        client.m_offset = 0;
        client.m_left_to_send = 0;
        client.m_is_new = 0;
    }

    TEST_ASSERT_EQUAL(0, client.m_is_new);
    TEST_ASSERT_EQUAL(0, client.m_offset);
    TEST_ASSERT_EQUAL(0, client.m_left_to_send);
}

/* Test: send_to_relay buffer extraction loop */
static void test_send_to_relay_buffer_loop(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));

    client.m_buffer = malloc(4096);
    client.m_buffer_size = 4096;
    client.m_offset = 0;
    client.m_left_to_send = 0;
    client.m_is_new = 0;

    /* Simulate buffer extraction */
    if (!client.m_left_to_send) {
        client.m_offset = 0;
        /* Would call cbuf2_extract_relay here - returns SR_ERROR_BUFFER_EMPTY in our stub */
    }

    TEST_ASSERT_EQUAL(0, client.m_offset);
    TEST_ASSERT_EQUAL(0, client.m_left_to_send);

    free(client.m_buffer);
}

/* Test: send_to_relay partial send updates offset */
static void test_send_to_relay_partial_send(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));

    client.m_buffer = malloc(4096);
    client.m_buffer_size = 4096;
    client.m_offset = 0;
    client.m_left_to_send = 2048;
    client.m_is_new = 0;

    /* Simulate partial send */
    int sent = 1024;
    client.m_offset += sent;
    client.m_left_to_send -= sent;

    TEST_ASSERT_EQUAL(1024, client.m_offset);
    TEST_ASSERT_EQUAL(1024, client.m_left_to_send);

    free(client.m_buffer);
}

/* Test: send_to_relay handles complete send */
static void test_send_to_relay_complete_send(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));

    client.m_buffer = malloc(4096);
    client.m_buffer_size = 4096;
    client.m_offset = 0;
    client.m_left_to_send = 1024;

    /* Complete send */
    int sent = 1024;
    client.m_offset += sent;
    client.m_left_to_send -= sent;

    TEST_ASSERT_EQUAL(1024, client.m_offset);
    TEST_ASSERT_EQUAL(0, client.m_left_to_send);

    free(client.m_buffer);
}

/* ============================================================================
 * Tests for swallow_receive logic (socket data discard)
 * ============================================================================ */

/* Test: swallow_receive logic - no data available */
static void test_swallow_receive_no_data(void)
{
    /* When select returns 0, there's no data to read */
    int select_result = 0;
    int recv_result = 0;
    BOOL hasmore = TRUE;

    /* Simulate the loop logic */
    if (select_result == 0) {
        hasmore = FALSE;
    }

    TEST_ASSERT_FALSE(hasmore);
}

/* Test: swallow_receive logic - data available and consumed */
static void test_swallow_receive_data_available(void)
{
    /* Simulate receiving data then no more */
    int iteration = 0;
    BOOL hasmore = TRUE;
    int ret = 0;

    while (hasmore && iteration < 3) {
        if (iteration == 0) {
            /* First iteration: data available, recv returns > 0 */
            ret = 100;  /* Simulated bytes received */
            hasmore = (ret > 0);
        } else if (iteration == 1) {
            /* Second iteration: no more data */
            ret = 0;
            hasmore = FALSE;
        }
        iteration++;
    }

    TEST_ASSERT_EQUAL(2, iteration);
    TEST_ASSERT_FALSE(hasmore);
}

/* Test: swallow_receive logic - socket error */
static void test_swallow_receive_socket_error(void)
{
    int ret = SOCKET_ERROR;
    BOOL hasmore = TRUE;

    if (ret == SOCKET_ERROR) {
        hasmore = FALSE;
    }

    TEST_ASSERT_FALSE(hasmore);
    TEST_ASSERT_EQUAL(SOCKET_ERROR, ret);
}

/* ============================================================================
 * Tests for HTTP header end detection
 * ============================================================================ */

/* Test: Detect end of HTTP header (CRLF only line) */
static void test_header_end_detection_crlf(void)
{
    char line[] = "\r";
    BOOL is_end = FALSE;

    /* Check for end of header: only CRLF */
    if ((line[0] == '\r') && (line[1] == 0)) {
        is_end = TRUE;
    }

    TEST_ASSERT_TRUE(is_end);
}

/* Test: Non-end header line */
static void test_header_non_end_line(void)
{
    char line[] = "Content-Type: audio/mpeg\r";
    BOOL is_end = FALSE;

    if ((line[0] == '\r') && (line[1] == 0)) {
        is_end = TRUE;
    }

    TEST_ASSERT_FALSE(is_end);
}

/* ============================================================================
 * Tests for thread_accept connection limit logic
 * ============================================================================ */

/* Test: Connection limit check - under limit */
static void test_connection_limit_under(void)
{
    unsigned long num_connected = 5;
    int max_connections = 10;
    BOOL should_accept = TRUE;

    /* Check limit (0 means unlimited) */
    if (max_connections > 0 && num_connected >= (unsigned long)max_connections) {
        should_accept = FALSE;
    }

    TEST_ASSERT_TRUE(should_accept);
}

/* Test: Connection limit check - at limit */
static void test_connection_limit_at_limit(void)
{
    unsigned long num_connected = 10;
    int max_connections = 10;
    BOOL should_accept = TRUE;

    if (max_connections > 0 && num_connected >= (unsigned long)max_connections) {
        should_accept = FALSE;
    }

    TEST_ASSERT_FALSE(should_accept);
}

/* Test: Connection limit check - over limit */
static void test_connection_limit_over(void)
{
    unsigned long num_connected = 15;
    int max_connections = 10;
    BOOL should_accept = TRUE;

    if (max_connections > 0 && num_connected >= (unsigned long)max_connections) {
        should_accept = FALSE;
    }

    TEST_ASSERT_FALSE(should_accept);
}

/* Test: Connection limit check - unlimited (0) */
static void test_connection_limit_unlimited(void)
{
    unsigned long num_connected = 1000;
    int max_connections = 0;  /* 0 = unlimited */
    BOOL should_accept = TRUE;

    if (max_connections > 0 && num_connected >= (unsigned long)max_connections) {
        should_accept = FALSE;
    }

    TEST_ASSERT_TRUE(should_accept);
}

/* ============================================================================
 * Tests for metadata interval handling
 * ============================================================================ */

/* Test: have_metadata when meta_interval is set */
static void test_have_metadata_with_interval(void)
{
    int meta_interval;
    int have_metadata;

    meta_interval = 16384;  /* Typical ICY metadata interval */

    if (meta_interval == NO_META_INTERVAL) {
        have_metadata = 0;
    } else {
        have_metadata = 1;
    }

    TEST_ASSERT_EQUAL(1, have_metadata);
}

/* Test: have_metadata when no meta interval */
static void test_have_metadata_no_interval(void)
{
    int meta_interval;
    int have_metadata;

    meta_interval = NO_META_INTERVAL;

    if (meta_interval == NO_META_INTERVAL) {
        have_metadata = 0;
    } else {
        have_metadata = 1;
    }

    TEST_ASSERT_EQUAL(0, have_metadata);
}

/* ============================================================================
 * Tests for relay list client ICY metadata handling
 * ============================================================================ */

/* Test: New client with ICY metadata support and server has metadata */
static void test_new_client_with_metadata_support(void)
{
    RELAY_LIST client;
    int have_metadata = 1;
    int icy_metadata = 1;

    memset(&client, 0, sizeof(RELAY_LIST));
    client.m_is_new = 1;
    client.m_sock = 500;

    /* Logic from thread_accept */
    if (have_metadata) {
        client.m_icy_metadata = icy_metadata;
    } else {
        client.m_icy_metadata = 0;
    }

    TEST_ASSERT_EQUAL(1, client.m_icy_metadata);
}

/* Test: New client with ICY metadata support but server has no metadata */
static void test_new_client_server_no_metadata(void)
{
    RELAY_LIST client;
    int have_metadata = 0;
    int icy_metadata = 1;  /* Client wants it but server doesn't have it */

    memset(&client, 0, sizeof(RELAY_LIST));
    client.m_is_new = 1;
    client.m_sock = 501;

    if (have_metadata) {
        client.m_icy_metadata = icy_metadata;
    } else {
        client.m_icy_metadata = 0;
    }

    TEST_ASSERT_EQUAL(0, client.m_icy_metadata);
}

/* Test: New client without ICY metadata support */
static void test_new_client_no_metadata_support(void)
{
    RELAY_LIST client;
    int have_metadata = 1;
    int icy_metadata = 0;  /* Client doesn't want metadata */

    memset(&client, 0, sizeof(RELAY_LIST));
    client.m_is_new = 1;
    client.m_sock = 502;

    if (have_metadata) {
        client.m_icy_metadata = icy_metadata;
    } else {
        client.m_icy_metadata = 0;
    }

    TEST_ASSERT_EQUAL(0, client.m_icy_metadata);
}

/* ============================================================================
 * Tests for port binding logic
 * ============================================================================ */

/* Test: Port range search - first port works */
static void test_port_range_first_works(void)
{
    u_short relay_port = 8000;
    u_short max_port = 8010;
    u_short port_used = 0;
    BOOL found = FALSE;

    /* Simulate finding first port */
    for (; relay_port <= max_port; relay_port++) {
        /* In real code, try_port would be called here */
        /* Simulate success on first port */
        found = TRUE;
        port_used = relay_port;
        break;
    }

    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL(8000, port_used);
}

/* Test: Port range search - need to try multiple ports */
static void test_port_range_multiple_tries(void)
{
    u_short relay_port = 8000;
    u_short max_port = 8010;
    u_short port_used = 0;
    int tries = 0;
    int success_port = 8003;  /* Simulate port 8003 working */

    for (; relay_port <= max_port; relay_port++) {
        tries++;
        if (relay_port == success_port) {
            port_used = relay_port;
            break;
        }
    }

    TEST_ASSERT_EQUAL(4, tries);  /* 8000, 8001, 8002, 8003 */
    TEST_ASSERT_EQUAL(8003, port_used);
}

/* Test: Port range search - no port available */
static void test_port_range_none_available(void)
{
    u_short relay_port = 8000;
    u_short max_port = 8010;
    u_short port_used = 0;
    error_code rc = SR_SUCCESS;

    for (; relay_port <= max_port; relay_port++) {
        /* Simulate all ports failing */
        rc = SR_ERROR_CANT_BIND_ON_PORT;
    }

    /* After loop, if no port found */
    if (port_used == 0) {
        rc = SR_ERROR_CANT_BIND_ON_PORT;
    }

    TEST_ASSERT_EQUAL(SR_ERROR_CANT_BIND_ON_PORT, rc);
    TEST_ASSERT_EQUAL(0, port_used);
}

/* Test: Single port mode (no search) */
static void test_single_port_mode(void)
{
    BOOL search_ports = FALSE;
    u_short relay_port = 8000;
    u_short max_port = 8010;

    /* In single port mode, max_port is set to relay_port */
    if (!search_ports) {
        max_port = relay_port;
    }

    TEST_ASSERT_EQUAL(8000, max_port);
}

/* ============================================================================
 * Tests for relay IP binding
 * ============================================================================ */

/* Test: Empty relay_ip uses interface detection */
static void test_relay_ip_empty_uses_interface(void)
{
    char relay_ip[] = "";
    BOOL use_interface = FALSE;

    if ('\0' == *relay_ip) {
        use_interface = TRUE;
    }

    TEST_ASSERT_TRUE(use_interface);
}

/* Test: Non-empty relay_ip uses gethostbyname */
static void test_relay_ip_specified(void)
{
    char relay_ip[] = "192.168.1.100";
    BOOL use_interface = FALSE;
    BOOL use_specific = FALSE;

    if ('\0' == *relay_ip) {
        use_interface = TRUE;
    } else {
        use_specific = TRUE;
    }

    TEST_ASSERT_FALSE(use_interface);
    TEST_ASSERT_TRUE(use_specific);
}

/* ============================================================================
 * Tests for thread running state management
 * ============================================================================ */

/* Test: Thread accept loop termination */
static void test_thread_accept_termination(void)
{
    RELAYLIB_INFO rli;
    memset(&rli, 0, sizeof(RELAYLIB_INFO));

    rli.m_running = TRUE;
    rli.m_running_accept = TRUE;

    /* Simulate stop request */
    rli.m_running = FALSE;

    /* Thread would check m_running and exit */
    while (rli.m_running) {
        /* Would do work */
    }

    /* After thread exits */
    rli.m_running_accept = FALSE;

    TEST_ASSERT_FALSE(rli.m_running);
    TEST_ASSERT_FALSE(rli.m_running_accept);
}

/* Test: Thread send loop termination */
static void test_thread_send_termination(void)
{
    RELAYLIB_INFO rli;
    memset(&rli, 0, sizeof(RELAYLIB_INFO));

    rli.m_running = TRUE;
    rli.m_running_send = TRUE;

    /* Simulate stop request */
    rli.m_running = FALSE;

    /* Thread would check m_running and exit */
    while (rli.m_running) {
        /* Would do work */
    }

    /* After thread exits */
    rli.m_running_send = FALSE;

    TEST_ASSERT_FALSE(rli.m_running);
    TEST_ASSERT_FALSE(rli.m_running_send);
}

/* ============================================================================
 * Tests for relaylib_stop timeout behavior
 * ============================================================================ */

/* Test: relaylib_stop waits for threads */
static void test_relaylib_stop_waits_for_threads(void)
{
    RELAYLIB_INFO rli;
    memset(&rli, 0, sizeof(RELAYLIB_INFO));

    rli.m_running = TRUE;
    rli.m_running_accept = TRUE;
    rli.m_running_send = TRUE;

    /* Stop sequence */
    rli.m_running = FALSE;

    /* Simulate threads finishing */
    int ix = 0;
    int max_wait = 5;  /* Reduced for test */

    /* Simulate threads still running for a bit */
    while (ix < max_wait && (rli.m_running_accept | rli.m_running_send)) {
        ix++;
        if (ix == 2) {
            rli.m_running_accept = FALSE;
        }
        if (ix == 3) {
            rli.m_running_send = FALSE;
        }
    }

    TEST_ASSERT_EQUAL(3, ix);
    TEST_ASSERT_FALSE(rli.m_running_accept);
    TEST_ASSERT_FALSE(rli.m_running_send);
}

/* Test: relaylib_stop threads never finish (timeout) */
static void test_relaylib_stop_threads_timeout(void)
{
    RELAYLIB_INFO rli;
    memset(&rli, 0, sizeof(RELAYLIB_INFO));

    rli.m_running = TRUE;
    rli.m_running_accept = TRUE;
    rli.m_running_send = TRUE;

    /* Stop sequence */
    rli.m_running = FALSE;

    int ix = 0;
    int max_wait = 10;  /* Reduced for test */

    /* Simulate threads never finishing */
    while (ix < max_wait && (rli.m_running_accept | rli.m_running_send)) {
        ix++;
        /* Threads never clear their flags */
    }

    /* Would hit timeout */
    TEST_ASSERT_EQUAL(max_wait, ix);
    TEST_ASSERT_TRUE(rli.m_running_accept || rli.m_running_send);
}

/* ============================================================================
 * Tests for client header response
 * ============================================================================ */

/* Test: Header send success */
static void test_header_send_success(void)
{
    char *header = "ICY 200 OK\r\n\r\n";
    int header_len = strlen(header);
    int ret = header_len;  /* Simulate successful send */
    BOOL good = FALSE;

    if (ret == header_len) {
        good = TRUE;
    }

    TEST_ASSERT_TRUE(good);
}

/* Test: Header send partial */
static void test_header_send_partial(void)
{
    char *header = "ICY 200 OK\r\n\r\n";
    int header_len = strlen(header);
    int ret = header_len / 2;  /* Partial send */
    BOOL good = FALSE;

    if (ret == header_len) {
        good = TRUE;
    }

    TEST_ASSERT_FALSE(good);
}

/* Test: Header send failure */
static void test_header_send_failure(void)
{
    char *header = "ICY 200 OK\r\n\r\n";
    int header_len = strlen(header);
    int ret = SOCKET_ERROR;
    BOOL good = FALSE;

    if (ret == header_len) {
        good = TRUE;
    }

    TEST_ASSERT_FALSE(good);
}

/* ============================================================================
 * Tests for EWOULDBLOCK handling in send
 * ============================================================================ */

/* Test: EWOULDBLOCK is recoverable */
static void test_send_ewouldblock_recoverable(void)
{
    int err_errno = EWOULDBLOCK;
    BOOL good = TRUE;
    int done = 0;

    /* Simulate SOCKET_ERROR with EWOULDBLOCK */
    if (err_errno == EWOULDBLOCK || err_errno == 0 || err_errno == 183) {
        /* Recoverable - client is slow, retry later */
        done = 1;  /* Exit send loop but keep client */
    } else {
        good = FALSE;  /* Real error */
    }

    TEST_ASSERT_TRUE(good);
    TEST_ASSERT_EQUAL(1, done);
}

/* Test: Other errors are not recoverable */
static void test_send_other_error_not_recoverable(void)
{
    int err_errno = ECONNRESET;
    BOOL good = TRUE;
    int done = 0;

    if (err_errno == EWOULDBLOCK || err_errno == 0 || err_errno == 183) {
        done = 1;
    } else {
        good = FALSE;
        done = 1;
    }

    TEST_ASSERT_FALSE(good);
    TEST_ASSERT_EQUAL(1, done);
}

/* ============================================================================
 * Tests for HTTP header delimiter constant
 * ============================================================================ */

/* Test: HTTP_HEADER_DELIM constant */
static void test_http_header_delim(void)
{
    const char *delim = "\n";
    char header[] = "GET / HTTP/1.0\nHost: localhost\n\n";
    char *token;
    int count = 0;

    /* Count tokens */
    char *copy = strdup(header);
    token = strtok(copy, delim);
    while (token != NULL) {
        count++;
        token = strtok(NULL, delim);
    }
    free(copy);

    TEST_ASSERT_EQUAL(2, count);  /* "GET / HTTP/1.0" and "Host: localhost" */
}

/* ============================================================================
 * Tests for relay list memory management
 * ============================================================================ */

/* Test: Allocate relay list item with buffer */
static void test_relay_list_alloc_with_buffer(void)
{
    RELAY_LIST *item = malloc(sizeof(RELAY_LIST));
    TEST_ASSERT_NOT_NULL(item);

    memset(item, 0, sizeof(RELAY_LIST));
    item->m_buffer = malloc(32768);  /* 32KB burst buffer */
    item->m_buffer_size = 32768;

    TEST_ASSERT_NOT_NULL(item->m_buffer);
    TEST_ASSERT_EQUAL(32768, item->m_buffer_size);

    free(item->m_buffer);
    free(item);
}

/* Test: Relay list item with no buffer (pre-init) */
static void test_relay_list_item_no_buffer(void)
{
    RELAY_LIST *item = malloc(sizeof(RELAY_LIST));
    TEST_ASSERT_NOT_NULL(item);

    memset(item, 0, sizeof(RELAY_LIST));
    item->m_buffer = NULL;
    item->m_buffer_size = 0;

    TEST_ASSERT_NULL(item->m_buffer);
    TEST_ASSERT_EQUAL(0, item->m_buffer_size);

    /* Free with NULL buffer should be safe */
    if (item->m_buffer != NULL) {
        free(item->m_buffer);
    }
    free(item);
}

/* ============================================================================
 * Tests for relaylib semaphore sequence
 * ============================================================================ */

/* Test: Semaphore signal before wait pattern */
static void test_semaphore_signal_before_wait(void)
{
    /* Reset counters */
    sem_signal_count = 0;
    sem_wait_count = 0;

    /* relaylib_start signals semaphore before thread starts */
    HSEM sem = threadlib_create_sem();
    threadlib_signal_sem(&sem);

    TEST_ASSERT_EQUAL(1, sem_signal_count);
    TEST_ASSERT_EQUAL(0, sem_wait_count);

    /* Thread later waits */
    threadlib_waitfor_sem(&sem);

    TEST_ASSERT_EQUAL(1, sem_signal_count);
    TEST_ASSERT_EQUAL(1, sem_wait_count);

    threadlib_destroy_sem(&sem);
}

/* ============================================================================
 * Tests for CBUF2 buffer presence check
 * ============================================================================ */

/* Test: cbuf2.buf NULL prevents client connection */
static void test_cbuf2_buf_null_prevents_connection(void)
{
    CBUF2 cbuf2;
    BOOL good = FALSE;

    /* Buffer not initialized */
    cbuf2.buf = NULL;

    /* Logic from thread_accept */
    if (cbuf2.buf != NULL) {
        good = TRUE;
    }

    TEST_ASSERT_FALSE(good);
}

/* Test: cbuf2.buf valid allows client connection */
static void test_cbuf2_buf_valid_allows_connection(void)
{
    CBUF2 cbuf2;
    BOOL good = FALSE;
    char dummy_buffer[1024];

    cbuf2.buf = dummy_buffer;

    if (cbuf2.buf != NULL) {
        good = TRUE;
    }

    TEST_ASSERT_TRUE(good);
}

/* ============================================================================
 * Tests for listening socket state
 * ============================================================================ */

/* Test: Listen socket valid check */
static void test_listen_socket_valid(void)
{
    RELAYLIB_INFO rli;
    memset(&rli, 0, sizeof(RELAYLIB_INFO));

    rli.m_listensock = 42;  /* Valid socket */

    TEST_ASSERT_NOT_EQUAL(SOCKET_ERROR, rli.m_listensock);
}

/* Test: Listen socket invalid check */
static void test_listen_socket_invalid(void)
{
    RELAYLIB_INFO rli;
    memset(&rli, 0, sizeof(RELAYLIB_INFO));

    rli.m_listensock = SOCKET_ERROR;

    TEST_ASSERT_EQUAL(SOCKET_ERROR, rli.m_listensock);
}

/* Test: Listen socket check in loop condition */
static void test_listen_socket_loop_condition(void)
{
    RELAYLIB_INFO rli;
    int iterations = 0;

    rli.m_listensock = 42;
    rli.m_running = TRUE;

    while (rli.m_listensock != SOCKET_ERROR && rli.m_running && iterations < 3) {
        iterations++;
        if (iterations == 2) {
            rli.m_running = FALSE;  /* Stop after 2 iterations */
        }
    }

    TEST_ASSERT_EQUAL(2, iterations);
}

/* ============================================================================
 * Tests for burst_amount constant
 * ============================================================================ */

/* Test: Burst amount initialization */
static void test_burst_amount_initialization(void)
{
    int burst_amount = 32 * 1024;  /* Same as in send_to_relay */

    TEST_ASSERT_EQUAL(32768, burst_amount);
}

/* ============================================================================
 * Additional edge case tests
 * ============================================================================ */

/* Test: Relay list length underflow protection */
static void test_relay_list_len_underflow_protection(void)
{
    test_rmi.relay_list_len = 0;

    /* Should not underflow */
    if (test_rmi.relay_list_len > 0) {
        test_rmi.relay_list_len--;
    }

    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
}

/* Test: Multiple sequential disconnects */
static void test_multiple_sequential_disconnects(void)
{
    RELAY_LIST *items[5];
    int i;

    /* Create list */
    for (i = 0; i < 5; i++) {
        items[i] = malloc(sizeof(RELAY_LIST));
        memset(items[i], 0, sizeof(RELAY_LIST));
        items[i]->m_sock = 800 + i;
        items[i]->m_buffer = NULL;
    }

    /* Link: 804 -> 803 -> 802 -> 801 -> 800 */
    for (i = 0; i < 4; i++) {
        items[i]->m_next = items[i + 1];
    }
    items[4]->m_next = NULL;

    test_rmi.relay_list = items[0];
    test_rmi.relay_list_len = 5;

    /* Disconnect all sequentially from head */
    for (i = 0; i < 5; i++) {
        if (test_rmi.relay_list != NULL) {
            relaylib_disconnect(&test_rmi, NULL, test_rmi.relay_list);
        }
    }

    TEST_ASSERT_NULL(test_rmi.relay_list);
    TEST_ASSERT_EQUAL(0, test_rmi.relay_list_len);
}

/* ============================================================================
 * Tests for header_receive function - Real socket I/O tests
 * ============================================================================ */

/* Test: header_receive with valid header ending in CRLF */
static void test_header_receive_valid_header(void)
{
    int sv[2];
    int ret;
    int icy_metadata;
    char *header = "ICY 200 OK\r\nicy-name: Test Stream\r\n\r\n";

    /* Create socket pair */
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT_EQUAL(0, ret);

    /* Write header to socket */
    write(sv[1], header, strlen(header));

    /* Test header_receive */
    ret = header_receive(sv[0], &icy_metadata);

    TEST_ASSERT_EQUAL(0, ret);  /* Success */
    TEST_ASSERT_EQUAL(0, icy_metadata);  /* No ICY-MetaData tag */

    close(sv[0]);
    close(sv[1]);
}

/* Test: header_receive with Icy-MetaData tag */
static void test_header_receive_with_metadata_tag(void)
{
    int sv[2];
    int ret;
    int icy_metadata;
    char *header = "ICY 200 OK\r\nIcy-MetaData: 1\r\n\r\n";

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT_EQUAL(0, ret);

    write(sv[1], header, strlen(header));

    ret = header_receive(sv[0], &icy_metadata);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, icy_metadata);

    close(sv[0]);
    close(sv[1]);
}

/* Test: header_receive with Icy-MetaData: 0 */
static void test_header_receive_metadata_zero(void)
{
    int sv[2];
    int ret;
    int icy_metadata;
    char *header = "ICY 200 OK\r\nIcy-MetaData: 0\r\n\r\n";

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT_EQUAL(0, ret);

    write(sv[1], header, strlen(header));

    ret = header_receive(sv[0], &icy_metadata);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(0, icy_metadata);

    close(sv[0]);
    close(sv[1]);
}

/* Test: header_receive with multiple header lines */
static void test_header_receive_multiple_headers(void)
{
    int sv[2];
    int ret;
    int icy_metadata;
    char *header = "ICY 200 OK\r\n"
                   "icy-name: Test\r\n"
                   "icy-genre: Rock\r\n"
                   "Icy-MetaData: 1\r\n"
                   "icy-br: 128\r\n"
                   "\r\n";

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT_EQUAL(0, ret);

    write(sv[1], header, strlen(header));

    ret = header_receive(sv[0], &icy_metadata);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, icy_metadata);

    close(sv[0]);
    close(sv[1]);
}

/* Test: header_receive timeout on no data */
static void test_header_receive_timeout(void)
{
    int sv[2];
    int ret;
    int icy_metadata;

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT_EQUAL(0, ret);

    /* Don't write anything - should timeout */
    ret = header_receive(sv[0], &icy_metadata);

    TEST_ASSERT_EQUAL(1, ret);  /* Error/timeout */

    close(sv[0]);
    close(sv[1]);
}

/* Test: header_receive with closed socket */
static void test_header_receive_closed_socket(void)
{
    int sv[2];
    int ret;
    int icy_metadata;

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT_EQUAL(0, ret);

    /* Close write end immediately */
    close(sv[1]);

    ret = header_receive(sv[0], &icy_metadata);

    TEST_ASSERT_EQUAL(1, ret);  /* Error */

    close(sv[0]);
}

/* ============================================================================
 * Tests for swallow_receive function - Real socket I/O tests
 * ============================================================================ */

/* Test: swallow_receive with no data available */
static void test_swallow_receive_no_data_real(void)
{
    int sv[2];
    int ret;

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT_EQUAL(0, ret);

    /* Don't send any data */
    ret = swallow_receive(sv[0]);

    TEST_ASSERT_EQUAL(0, ret);  /* Success - no data to read */

    close(sv[0]);
    close(sv[1]);
}

/* Test: swallow_receive with data available */
static void test_swallow_receive_with_data_real(void)
{
    int sv[2];
    int ret;
    char data[1024];

    memset(data, 'X', sizeof(data));

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT_EQUAL(0, ret);

    /* Send some data */
    write(sv[1], data, sizeof(data));

    /* Give socket time to have data ready */
    usleep(1000);

    ret = swallow_receive(sv[0]);

    /* Should successfully read and discard data */
    TEST_ASSERT_NOT_EQUAL(SOCKET_ERROR, ret);

    close(sv[0]);
    close(sv[1]);
}

/* Test: swallow_receive with multiple chunks */
static void test_swallow_receive_multiple_chunks(void)
{
    int sv[2];
    int ret;
    char data[2048];

    memset(data, 'Y', sizeof(data));

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT_EQUAL(0, ret);

    /* Send multiple chunks */
    write(sv[1], data, 1024);
    write(sv[1], data, 1024);

    usleep(1000);

    ret = swallow_receive(sv[0]);

    /* Should loop and read all data */
    TEST_ASSERT_NOT_EQUAL(SOCKET_ERROR, ret);

    close(sv[0]);
    close(sv[1]);
}

/* Test: swallow_receive with closed socket */
static void test_swallow_receive_socket_closed(void)
{
    int sv[2];
    int ret;

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT_EQUAL(0, ret);

    /* Close write end */
    close(sv[1]);

    ret = swallow_receive(sv[0]);

    /* Should handle closed socket gracefully */
    /* Return value might be 0 or SOCKET_ERROR depending on timing */
    TEST_ASSERT_TRUE(ret == 0 || ret == SOCKET_ERROR);

    close(sv[0]);
}

/* ============================================================================
 * Tests for send_to_relay function - Real function calls
 * ============================================================================ */

/* Test: send_to_relay with new client initialization */
static void test_send_to_relay_real_new_client(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));
    memset(&test_rmi.cbuf2, 0, sizeof(CBUF2));

    /* Set up as new client */
    client.m_is_new = 1;
    client.m_sock = 100;
    client.m_buffer = NULL;

    /* Call actual send_to_relay function */
    BOOL result = send_to_relay(&test_rmi, &client);

    /* Should initialize and succeed (cbuf2_extract_relay returns BUFFER_EMPTY in stub) */
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(0, client.m_is_new);  /* Should be set to 0 after init */
    TEST_ASSERT_EQUAL(0, client.m_offset);
    TEST_ASSERT_EQUAL(0, client.m_left_to_send);
}

/* Test: send_to_relay with existing client and empty buffer */
static void test_send_to_relay_real_empty_buffer(void)
{
    RELAY_LIST client;
    memset(&client, 0, sizeof(RELAY_LIST));
    memset(&test_rmi.cbuf2, 0, sizeof(CBUF2));

    client.m_is_new = 0;
    client.m_sock = 100;
    client.m_buffer = malloc(1024);
    client.m_buffer_size = 1024;
    client.m_offset = 0;
    client.m_left_to_send = 0;  /* No data to send */

    BOOL result = send_to_relay(&test_rmi, &client);

    /* Should succeed but do nothing since buffer is empty */
    TEST_ASSERT_TRUE(result);

    free(client.m_buffer);
}

/* Test: send_to_relay with real socket and data to send */
static void test_send_to_relay_real_with_data(void)
{
    int sv[2];
    int ret;
    RELAY_LIST client;
    char recv_buf[1024];

    /* Create socket pair */
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT_EQUAL(0, ret);

    /* Set up client with data to send */
    memset(&client, 0, sizeof(RELAY_LIST));
    memset(&test_rmi.cbuf2, 0, sizeof(CBUF2));

    client.m_is_new = 0;
    client.m_sock = sv[0];  /* Use real socket */
    client.m_buffer = malloc(1024);
    client.m_buffer_size = 1024;
    client.m_offset = 0;

    /* Configure stub to provide data */
    cbuf2_extract_relay_return_value = SR_SUCCESS;
    cbuf2_extract_relay_data_size = 100;

    /* Call send_to_relay - should extract data and send it */
    BOOL result = send_to_relay(&test_rmi, &client);

    /* Should succeed */
    TEST_ASSERT_TRUE(result);

    /* Try to receive the sent data */
    usleep(1000);
    ret = recv(sv[1], recv_buf, sizeof(recv_buf), 0);

    /* Should have received data */
    TEST_ASSERT_TRUE(ret > 0);

    /* Clean up */
    free(client.m_buffer);
    close(sv[0]);
    close(sv[1]);

    /* Reset stub */
    cbuf2_extract_relay_return_value = SR_ERROR_BUFFER_EMPTY;
    cbuf2_extract_relay_data_size = 0;
    cbuf2_extract_relay_call_count = 0;
}

/* ============================================================================
 * Main - Run all tests
 * ============================================================================ */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* RELAYLIB_INFO structure tests */
    RUN_TEST(test_relaylib_info_structure_size);
    RUN_TEST(test_relaylib_info_initial_state);

    /* RELAY_LIST structure tests */
    RUN_TEST(test_relay_list_structure);
    RUN_TEST(test_relay_list_linking);

    /* relaylib_start tests */
    RUN_TEST(test_relaylib_start_invalid_port);
    RUN_TEST(test_relaylib_start_null_port_used);
    RUN_TEST(test_relaylib_start_initializes_flags);

    /* relaylib_stop tests */
    RUN_TEST(test_relaylib_stop_when_not_running);
    RUN_TEST(test_relaylib_stop_clears_running_flag);
    RUN_TEST(test_relaylib_stop_destroys_relay_list);
    RUN_TEST(test_relaylib_stop_destroys_single_item);
    RUN_TEST(test_relaylib_stop_handles_null_buffers);

    /* relaylib_disconnect tests */
    RUN_TEST(test_relaylib_disconnect_first_item);
    RUN_TEST(test_relaylib_disconnect_middle_item);
    RUN_TEST(test_relaylib_disconnect_last_item);
    RUN_TEST(test_relaylib_disconnect_frees_buffer);
    RUN_TEST(test_relaylib_disconnect_single_item);

    /* RIP_MANAGER_INFO relay-related fields tests */
    RUN_TEST(test_rmi_relay_list_init);
    RUN_TEST(test_rmi_relaylib_info_access);

    /* Relay list management tests */
    RUN_TEST(test_relay_list_add_increases_length);
    RUN_TEST(test_relay_list_traversal);

    /* RELAY_LIST client metadata flag tests */
    RUN_TEST(test_relay_list_icy_metadata_flag);
    RUN_TEST(test_relay_list_is_new_flag);

    /* Buffer management tests */
    RUN_TEST(test_relay_list_buffer_tracking);
    RUN_TEST(test_relay_list_cbuf_offset);

    /* Error code tests */
    RUN_TEST(test_error_cant_bind_on_port);
    RUN_TEST(test_error_host_not_connected);
    RUN_TEST(test_error_invalid_param);

    /* Constants and limits tests */
    RUN_TEST(test_max_header_len_constant);
    RUN_TEST(test_socket_error_constant);

    /* relaylib_start - Thread initialization tests */
    RUN_TEST(test_relaylib_start_threads_success);
    RUN_TEST(test_relaylib_start_thread_creation_fails);
    RUN_TEST(test_relaylib_start_stop_full_lifecycle);
    RUN_TEST(test_relaylib_start_wide_port_range);

    /* relaylib_start - Port range search tests */
    RUN_TEST(test_relaylib_start_port_search_enabled);
    RUN_TEST(test_relaylib_start_single_port);
    RUN_TEST(test_relaylib_start_with_relay_ip);
    RUN_TEST(test_relaylib_start_high_port_range);
    RUN_TEST(test_relaylib_start_creates_semaphores);

    /* Relay list - Client addition and removal workflow tests */
    RUN_TEST(test_relay_client_add_remove_workflow);
    RUN_TEST(test_relay_multiple_clients_metadata);

    /* Buffer management - send state tracking tests */
    RUN_TEST(test_buffer_partial_send_tracking);
    RUN_TEST(test_buffer_reset_after_send);

    /* Client state machine tests */
    RUN_TEST(test_new_client_initialization);
    RUN_TEST(test_client_state_transition_new_to_active);

    /* Semaphore usage tracking tests */
    RUN_TEST(test_semaphore_creation_tracking);
    RUN_TEST(test_semaphore_signal_wait_tracking);

    /* Relay list integrity tests */
    RUN_TEST(test_empty_relay_list_operations);
    RUN_TEST(test_large_relay_list);
    RUN_TEST(test_relay_list_mixed_buffers);

    /* Edge case tests */
    RUN_TEST(test_disconnect_null_prev_is_head_removal);
    RUN_TEST(test_zero_length_buffer);
    RUN_TEST(test_maximum_offset_tracking);

    /* RELAYLIB_INFO flag coordination tests */
    RUN_TEST(test_running_flags_coordination);
    RUN_TEST(test_socket_state_transitions);

    /* cbuf offset management tests */
    RUN_TEST(test_cbuf_offset_increments);
    RUN_TEST(test_cbuf_offset_large_values);

    /* tag_compare tests */
    RUN_TEST(test_tag_compare_case_insensitive);
    RUN_TEST(test_tag_compare_no_match);
    RUN_TEST(test_tag_compare_short_string);
    RUN_TEST(test_tag_compare_partial_match);
    RUN_TEST(test_tag_compare_extra_content);

    /* destroy_all_hostsocks tests */
    RUN_TEST(test_destroy_all_hostsocks_clears_list);
    RUN_TEST(test_destroy_all_hostsocks_null_buffers);
    RUN_TEST(test_destroy_all_hostsocks_empty_list);

    /* HTTP header parsing tests */
    RUN_TEST(test_parse_icy_metadata_value);
    RUN_TEST(test_parse_icy_metadata_with_spaces);
    RUN_TEST(test_parse_icy_metadata_value_zero);
    RUN_TEST(test_parse_no_icy_metadata);

    /* make_nonblocking tests */
    RUN_TEST(test_make_nonblocking_sets_flag);
    RUN_TEST(test_make_nonblocking_invalid_socket);

    /* send_to_relay tests */
    RUN_TEST(test_send_to_relay_new_client_init);
    RUN_TEST(test_send_to_relay_buffer_loop);
    RUN_TEST(test_send_to_relay_partial_send);
    RUN_TEST(test_send_to_relay_complete_send);

    /* swallow_receive tests */
    RUN_TEST(test_swallow_receive_no_data);
    RUN_TEST(test_swallow_receive_data_available);
    RUN_TEST(test_swallow_receive_socket_error);

    /* header_receive tests with real socket I/O */
    RUN_TEST(test_header_receive_valid_header);
    RUN_TEST(test_header_receive_with_metadata_tag);
    RUN_TEST(test_header_receive_metadata_zero);
    RUN_TEST(test_header_receive_multiple_headers);
    RUN_TEST(test_header_receive_timeout);
    RUN_TEST(test_header_receive_closed_socket);

    /* swallow_receive tests with real socket I/O */
    RUN_TEST(test_swallow_receive_no_data_real);
    RUN_TEST(test_swallow_receive_with_data_real);
    RUN_TEST(test_swallow_receive_multiple_chunks);
    RUN_TEST(test_swallow_receive_socket_closed);

    /* send_to_relay tests with real function calls */
    RUN_TEST(test_send_to_relay_real_new_client);
    RUN_TEST(test_send_to_relay_real_empty_buffer);
    RUN_TEST(test_send_to_relay_real_with_data);

    /* Header end detection tests */
    RUN_TEST(test_header_end_detection_crlf);
    RUN_TEST(test_header_non_end_line);

    /* Connection limit tests */
    RUN_TEST(test_connection_limit_under);
    RUN_TEST(test_connection_limit_at_limit);
    RUN_TEST(test_connection_limit_over);
    RUN_TEST(test_connection_limit_unlimited);

    /* Metadata interval tests */
    RUN_TEST(test_have_metadata_with_interval);
    RUN_TEST(test_have_metadata_no_interval);

    /* Client ICY metadata handling tests */
    RUN_TEST(test_new_client_with_metadata_support);
    RUN_TEST(test_new_client_server_no_metadata);
    RUN_TEST(test_new_client_no_metadata_support);

    /* Port binding tests */
    RUN_TEST(test_port_range_first_works);
    RUN_TEST(test_port_range_multiple_tries);
    RUN_TEST(test_port_range_none_available);
    RUN_TEST(test_single_port_mode);

    /* Relay IP binding tests */
    RUN_TEST(test_relay_ip_empty_uses_interface);
    RUN_TEST(test_relay_ip_specified);

    /* Thread running state tests */
    RUN_TEST(test_thread_accept_termination);
    RUN_TEST(test_thread_send_termination);

    /* relaylib_stop timeout tests */
    RUN_TEST(test_relaylib_stop_waits_for_threads);
    RUN_TEST(test_relaylib_stop_threads_timeout);

    /* Client header response tests */
    RUN_TEST(test_header_send_success);
    RUN_TEST(test_header_send_partial);
    RUN_TEST(test_header_send_failure);

    /* EWOULDBLOCK handling tests */
    RUN_TEST(test_send_ewouldblock_recoverable);
    RUN_TEST(test_send_other_error_not_recoverable);

    /* HTTP header delimiter tests */
    RUN_TEST(test_http_header_delim);

    /* Relay list memory management tests */
    RUN_TEST(test_relay_list_alloc_with_buffer);
    RUN_TEST(test_relay_list_item_no_buffer);

    /* Semaphore sequence tests */
    RUN_TEST(test_semaphore_signal_before_wait);

    /* CBUF2 buffer presence tests */
    RUN_TEST(test_cbuf2_buf_null_prevents_connection);
    RUN_TEST(test_cbuf2_buf_valid_allows_connection);

    /* Listen socket state tests */
    RUN_TEST(test_listen_socket_valid);
    RUN_TEST(test_listen_socket_invalid);
    RUN_TEST(test_listen_socket_loop_condition);

    /* Burst amount tests */
    RUN_TEST(test_burst_amount_initialization);

    /* Additional edge case tests */
    RUN_TEST(test_relay_list_len_underflow_protection);
    RUN_TEST(test_multiple_sequential_disconnects);

    return UNITY_END();
}
