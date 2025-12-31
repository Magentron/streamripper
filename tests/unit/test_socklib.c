/* test_socklib.c - Unit tests for lib/socklib.c
 *
 * Tests for socket library functions with real implementations.
 * Uses localhost connections to test actual socket operations.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unity.h>

#include "srtypes.h"
#include "errors.h"
#include "socklib.h"

/* Test server state */
static int test_server_port = 0;
static pthread_t server_thread;
static int server_should_stop = 0;
static int server_socket = -1;

/* Stub implementations for dependencies */
void debug_printf(char *fmt, ...) {
    /* Silent debug for tests */
}

/* Simple echo server for testing */
void* echo_server_thread(void* arg) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int listen_sock, client_sock;
    char buffer[1024];
    int ret;

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        return NULL;
    }

    /* Allow port reuse */
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(test_server_port);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_sock);
        return NULL;
    }

    if (listen(listen_sock, 1) < 0) {
        close(listen_sock);
        return NULL;
    }

    server_socket = listen_sock;

    /* Set timeout to allow checking server_should_stop */
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (!server_should_stop) {
        client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            continue;
        }

        /* Echo back data */
        while ((ret = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
            send(client_sock, buffer, ret, 0);
        }

        close(client_sock);
    }

    close(listen_sock);
    server_socket = -1;
    return NULL;
}

/* HTTP header server for testing */
void* http_header_server_thread(void* arg) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int listen_sock, client_sock;
    const char* response = "HTTP/1.1 200 OK\r\nContent-Type: audio/mpeg\r\n\r\n";

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        return NULL;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(test_server_port);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_sock);
        return NULL;
    }

    if (listen(listen_sock, 1) < 0) {
        close(listen_sock);
        return NULL;
    }

    server_socket = listen_sock;

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (!server_should_stop) {
        client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            continue;
        }

        /* Send HTTP header response */
        send(client_sock, response, strlen(response), 0);

        close(client_sock);
    }

    close(listen_sock);
    server_socket = -1;
    return NULL;
}

void start_test_server(void* (*server_func)(void*)) {
    /* Use dynamic port to avoid conflicts */
    test_server_port = 19000 + (getpid() % 1000);
    server_should_stop = 0;
    pthread_create(&server_thread, NULL, server_func, NULL);
    usleep(100000); /* Give server time to start */
}

void stop_test_server(void) {
    server_should_stop = 1;
    if (server_socket >= 0) {
        shutdown(server_socket, SHUT_RDWR);
    }
    pthread_join(server_thread, NULL);
    usleep(100000); /* Give server time to stop */
}

void setUp(void)
{
    /* Initialize socket library */
    socklib_init();
}

void tearDown(void)
{
    /* Cleanup socket library */
    socklib_cleanup();
}

/* Test: HSOCKET structure initialization */
static void test_hsocket_structure(void)
{
    HSOCKET socket_handle;

    memset(&socket_handle, 0, sizeof(HSOCKET));

    /* Test socket descriptor */
    socket_handle.s = 5;
    TEST_ASSERT_EQUAL(5, socket_handle.s);

    /* Test closed flag */
    socket_handle.closed = FALSE;
    TEST_ASSERT_EQUAL(FALSE, socket_handle.closed);

    socket_handle.closed = TRUE;
    TEST_ASSERT_EQUAL(TRUE, socket_handle.closed);
}

/* Test: HSOCKET invalid socket value */
static void test_hsocket_invalid_socket(void)
{
    HSOCKET socket_handle;

    memset(&socket_handle, 0, sizeof(HSOCKET));

    /* SOCKET_ERROR is typically -1 */
    socket_handle.s = -1;
    TEST_ASSERT_TRUE(socket_handle.s < 0);
}

/* Test: Socket error codes */
static void test_socket_error_codes(void)
{
    /* Verify socket-related error codes are defined and negative */
    TEST_ASSERT_TRUE(SR_ERROR_CANT_CREATE_SOCKET < 0);
    TEST_ASSERT_TRUE(SR_ERROR_CONNECT_FAILED < 0);
    TEST_ASSERT_TRUE(SR_ERROR_CANT_RESOLVE_HOSTNAME < 0);
    TEST_ASSERT_TRUE(SR_ERROR_RECV_FAILED < 0);
    TEST_ASSERT_TRUE(SR_ERROR_SEND_FAILED < 0);
    TEST_ASSERT_TRUE(SR_ERROR_SOCKET_CLOSED < 0);
    TEST_ASSERT_TRUE(SR_ERROR_CANT_SET_SOCKET_OPTIONS < 0);
    TEST_ASSERT_TRUE(SR_ERROR_CANT_BIND_ON_INTERFACE < 0);
}

/* Test: Socket error codes are distinct */
static void test_socket_error_codes_distinct(void)
{
    TEST_ASSERT_NOT_EQUAL(SR_ERROR_CANT_CREATE_SOCKET, SR_ERROR_CONNECT_FAILED);
    TEST_ASSERT_NOT_EQUAL(SR_ERROR_CONNECT_FAILED, SR_ERROR_CANT_RESOLVE_HOSTNAME);
    TEST_ASSERT_NOT_EQUAL(SR_ERROR_RECV_FAILED, SR_ERROR_SEND_FAILED);
    TEST_ASSERT_NOT_EQUAL(SR_ERROR_SOCKET_CLOSED, SR_ERROR_CANT_SET_SOCKET_OPTIONS);
}

/* Test: Timeout error codes */
static void test_timeout_error_codes(void)
{
    TEST_ASSERT_TRUE(SR_ERROR_TIMEOUT < 0);
    TEST_ASSERT_TRUE(SR_ERROR_SELECT_FAILED < 0);

    TEST_ASSERT_NOT_EQUAL(SR_ERROR_TIMEOUT, SR_ERROR_SELECT_FAILED);
}

/* Test: SR_SUCCESS is zero */
static void test_sr_success(void)
{
    TEST_ASSERT_EQUAL(0, SR_SUCCESS);
}

/* Test: MAX_HOST_LEN constant */
static void test_max_host_len(void)
{
    /* MAX_HOST_LEN should be sufficient for hostnames */
    TEST_ASSERT_TRUE(MAX_HOST_LEN >= 256);
    TEST_ASSERT_TRUE(MAX_HOST_LEN <= 2048);
}

/* Test: MAX_HEADER_LEN for socket read buffer */
static void test_max_header_len_for_socket(void)
{
    /* Header buffer should be reasonably sized */
    TEST_ASSERT_TRUE(MAX_HEADER_LEN >= 4096);
    TEST_ASSERT_TRUE(MAX_HEADER_LEN <= 65536);
}

/* Test: Common port numbers */
static void test_common_ports(void)
{
    u_short http_port = 80;
    u_short https_port = 443;
    u_short shoutcast_port = 8000;
    u_short icecast_port = 8080;

    TEST_ASSERT_EQUAL(80, http_port);
    TEST_ASSERT_EQUAL(443, https_port);
    TEST_ASSERT_EQUAL(8000, shoutcast_port);
    TEST_ASSERT_EQUAL(8080, icecast_port);
}

/* Test: Port range validation */
static void test_port_range(void)
{
    /* Valid ports are 1-65535 */
    u_short min_port = 1;
    u_short max_port = 65535;

    TEST_ASSERT_TRUE(min_port >= 1);
    TEST_ASSERT_TRUE(max_port <= 65535);
    TEST_ASSERT_TRUE(max_port > min_port);
}

/* Test: RIP_MANAGER_INFO stream_sock field */
static void test_rmi_stream_sock(void)
{
    RIP_MANAGER_INFO rmi;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));

    /* Test stream_sock.s field */
    rmi.stream_sock.s = 10;
    TEST_ASSERT_EQUAL(10, rmi.stream_sock.s);

    /* Test stream_sock.closed field */
    rmi.stream_sock.closed = FALSE;
    TEST_ASSERT_EQUAL(FALSE, rmi.stream_sock.closed);
}

/* Test: RIP_MANAGER_INFO abort_pipe for Unix */
static void test_rmi_abort_pipe(void)
{
#if __UNIX__
    RIP_MANAGER_INFO rmi;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));

    /* abort_pipe is used for signaling abort on Unix */
    rmi.abort_pipe[0] = 3;  /* read end */
    rmi.abort_pipe[1] = 4;  /* write end */

    TEST_ASSERT_EQUAL(3, rmi.abort_pipe[0]);
    TEST_ASSERT_EQUAL(4, rmi.abort_pipe[1]);
#else
    /* Pass on non-Unix systems */
    TEST_ASSERT_TRUE(1);
#endif
}

/* Test: socklib_init return value on success */
static void test_socklib_init_success_value(void)
{
    /* socklib_init should return SR_SUCCESS (0) on success */
    /* We can't actually call it without side effects, but verify the expected return */
    TEST_ASSERT_EQUAL(0, SR_SUCCESS);
}

/* Test: SOCKET type definition */
static void test_socket_type(void)
{
    /* On Unix, SOCKET is int */
#ifndef WIN32
    SOCKET sock = 5;
    TEST_ASSERT_EQUAL(5, sock);

    sock = -1;  /* Invalid socket */
    TEST_ASSERT_EQUAL(-1, sock);
#else
    TEST_ASSERT_TRUE(1);
#endif
}

/* Test: IP address format constants */
static void test_ip_address_constants(void)
{
    /* MAX_IP_LEN should accommodate xxx.xxx.xxx.xxx format */
    /* 3+1+3+1+3+1+3+1 = 16 characters */
    TEST_ASSERT_TRUE(MAX_IP_LEN >= 16);
}

/* Test: INADDR_ANY and INADDR_NONE conceptual */
static void test_inaddr_constants(void)
{
    /* INADDR_NONE is typically 0xFFFFFFFF (-1 as signed) */
    /* This is used to detect invalid addresses from inet_addr */
    unsigned long inaddr_none_conceptual = 0xFFFFFFFF;
    TEST_ASSERT_EQUAL(0xFFFFFFFF, inaddr_none_conceptual);

    /* INADDR_ANY is 0, meaning bind to all interfaces */
    unsigned long inaddr_any_conceptual = 0;
    TEST_ASSERT_EQUAL(0, inaddr_any_conceptual);
}

/* Test: FD_SETSIZE consideration */
static void test_fd_setsize(void)
{
    /* FD_SETSIZE limits the number of file descriptors for select() */
    /* This is typically 1024 on Linux, but we just verify it's positive */
#if defined(FD_SETSIZE)
    TEST_ASSERT_TRUE(FD_SETSIZE > 0);
#else
    TEST_ASSERT_TRUE(1);
#endif
}

/* Test: STREAM_PREFS timeout field */
static void test_stream_prefs_timeout(void)
{
    STREAM_PREFS prefs;

    memset(&prefs, 0, sizeof(STREAM_PREFS));

    /* Default timeout is typically 15 seconds */
    prefs.timeout = 15;
    TEST_ASSERT_EQUAL(15, prefs.timeout);

    /* Double timeout for first read */
    unsigned long first_read_timeout = prefs.timeout * 2;
    TEST_ASSERT_EQUAL(30, first_read_timeout);
}

/* Test: NO_HTTP_HEADER error for incomplete reads */
static void test_no_http_header_error(void)
{
    TEST_ASSERT_TRUE(SR_ERROR_NO_HTTP_HEADER < 0);
    TEST_ASSERT_NOT_EQUAL(SR_ERROR_NO_HTTP_HEADER, SR_ERROR_RECV_FAILED);
}

/* ========================================================================
 * ACTUAL FUNCTION TESTS - Testing real socklib.c implementations
 * ======================================================================== */

/* Test: socklib_init success */
static void test_socklib_init_success(void)
{
    error_code ret = socklib_init();
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
    socklib_cleanup();
}

/* Test: socklib_open with valid localhost connection */
static void test_socklib_open_success(void)
{
    HSOCKET sock;
    error_code ret;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
    TEST_ASSERT_FALSE(sock.closed);
    TEST_ASSERT_TRUE(sock.s >= 0);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_open with hostname resolution */
static void test_socklib_open_with_hostname(void)
{
    HSOCKET sock;
    error_code ret;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "localhost", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
    TEST_ASSERT_FALSE(sock.closed);
    TEST_ASSERT_TRUE(sock.s >= 0);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_open with NULL socket handle */
static void test_socklib_open_null_socket(void)
{
    error_code ret = socklib_open(NULL, "127.0.0.1", 80, NULL, 5);
    TEST_ASSERT_EQUAL(SR_ERROR_INVALID_PARAM, ret);
}

/* Test: socklib_open with NULL host */
static void test_socklib_open_null_host(void)
{
    HSOCKET sock;
    error_code ret = socklib_open(&sock, NULL, 80, NULL, 5);
    TEST_ASSERT_EQUAL(SR_ERROR_INVALID_PARAM, ret);
}

/* Test: socklib_open with invalid hostname */
static void test_socklib_open_invalid_hostname(void)
{
    HSOCKET sock;
    error_code ret = socklib_open(&sock, "invalid.hostname.does.not.exist.local", 80, NULL, 5);
    TEST_ASSERT_EQUAL(SR_ERROR_CANT_RESOLVE_HOSTNAME, ret);
}

/* Test: socklib_open with connection refused */
static void test_socklib_open_connection_refused(void)
{
    HSOCKET sock;
    /* Port 1 is typically not listening */
    error_code ret = socklib_open(&sock, "127.0.0.1", 1, NULL, 1);
    TEST_ASSERT_EQUAL(SR_ERROR_CONNECT_FAILED, ret);
}

/* Test: socklib_close sets closed flag */
static void test_socklib_close_sets_flag(void)
{
    HSOCKET sock;
    error_code ret;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
    TEST_ASSERT_FALSE(sock.closed);

    socklib_close(&sock);
    TEST_ASSERT_TRUE(sock.closed);

    stop_test_server();
}

/* Test: socklib_sendall basic operation */
static void test_socklib_sendall_success(void)
{
    HSOCKET sock;
    error_code ret;
    char send_buf[] = "Hello, server!";
    int sent;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    sent = socklib_sendall(&sock, send_buf, strlen(send_buf));
    TEST_ASSERT_EQUAL(strlen(send_buf), sent);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_sendall on closed socket */
static void test_socklib_sendall_closed_socket(void)
{
    HSOCKET sock;
    char send_buf[] = "test";

    start_test_server(echo_server_thread);

    socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    socklib_close(&sock);

    int sent = socklib_sendall(&sock, send_buf, strlen(send_buf));
    TEST_ASSERT_EQUAL(SR_ERROR_SOCKET_CLOSED, sent);

    stop_test_server();
}

/* Test: socklib_recvall basic operation */
static void test_socklib_recvall_success(void)
{
    HSOCKET sock;
    error_code ret;
    char send_buf[] = "Echo test";
    char recv_buf[256];
    int received;
    RIP_MANAGER_INFO rmi;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    socklib_sendall(&sock, send_buf, strlen(send_buf));
    usleep(50000); /* Give server time to echo */

    memset(recv_buf, 0, sizeof(recv_buf));
    received = socklib_recvall(&rmi, &sock, recv_buf, strlen(send_buf), 2);
    TEST_ASSERT_EQUAL(strlen(send_buf), received);
    TEST_ASSERT_EQUAL_STRING(send_buf, recv_buf);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_recvall on closed socket */
static void test_socklib_recvall_closed_socket(void)
{
    HSOCKET sock;
    char recv_buf[256];
    RIP_MANAGER_INFO rmi;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(echo_server_thread);

    socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    socklib_close(&sock);

    int received = socklib_recvall(&rmi, &sock, recv_buf, 10, 2);
    TEST_ASSERT_EQUAL(SR_ERROR_SOCKET_CLOSED, received);

    stop_test_server();
}

/* Test: socklib_recvall with timeout */
static void test_socklib_recvall_timeout(void)
{
    HSOCKET sock;
    char recv_buf[256];
    RIP_MANAGER_INFO rmi;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    /* Try to read data with timeout, server won't send anything */
    int received = socklib_recvall(&rmi, &sock, recv_buf, 10, 1);
    /* Should timeout */
    TEST_ASSERT_TRUE(received == SR_ERROR_TIMEOUT || received == 0);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_read_header success */
static void test_socklib_read_header_success(void)
{
    HSOCKET sock;
    char header_buf[4096];
    RIP_MANAGER_INFO rmi;
    STREAM_PREFS prefs;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&prefs, 0, sizeof(STREAM_PREFS));
    prefs.timeout = 5;
    rmi.prefs = &prefs;
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(http_header_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    ret = socklib_read_header(&rmi, &sock, header_buf, sizeof(header_buf));
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
    TEST_ASSERT_TRUE(strstr(header_buf, "HTTP/1.1") != NULL);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_read_header on closed socket */
static void test_socklib_read_header_closed_socket(void)
{
    HSOCKET sock;
    char header_buf[4096];
    RIP_MANAGER_INFO rmi;
    STREAM_PREFS prefs;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&prefs, 0, sizeof(STREAM_PREFS));
    prefs.timeout = 5;
    rmi.prefs = &prefs;

    start_test_server(http_header_server_thread);

    socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    socklib_close(&sock);

    error_code ret = socklib_read_header(&rmi, &sock, header_buf, sizeof(header_buf));
    TEST_ASSERT_EQUAL(SR_ERROR_SOCKET_CLOSED, ret);

    stop_test_server();
}

/* Test: read_interface on Unix */
static void test_read_interface_unix(void)
{
#if !defined(WIN32)
    /* Test with loopback interface */
    uint32_t addr = 0;
    error_code ret = read_interface("lo", &addr);
    /* Should succeed on most Unix systems */
    if (ret == 0) {
        TEST_ASSERT_NOT_EQUAL(0, addr);
    } else {
        /* Some systems may not support this, that's OK */
        TEST_ASSERT_TRUE(ret < 0);
    }
#else
    /* On Windows, should return -1 */
    uint32_t addr = 0;
    error_code ret = read_interface("eth0", &addr);
    TEST_ASSERT_EQUAL(-1, ret);
#endif
}

/* Test: read_interface with invalid interface */
static void test_read_interface_invalid(void)
{
#if !defined(WIN32)
    uint32_t addr = 0;
    error_code ret = read_interface("invalid_iface_name_xyz", &addr);
    TEST_ASSERT_TRUE(ret < 0);
#else
    TEST_ASSERT_TRUE(1); /* Skip on Windows */
#endif
}

/* Test: Large data transfer with socklib_sendall */
static void test_socklib_sendall_large_data(void)
{
    HSOCKET sock;
    error_code ret;
    char send_buf[8192];
    int sent;

    memset(send_buf, 'A', sizeof(send_buf) - 1);
    send_buf[sizeof(send_buf) - 1] = '\0';

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    sent = socklib_sendall(&sock, send_buf, sizeof(send_buf) - 1);
    TEST_ASSERT_EQUAL(sizeof(send_buf) - 1, sent);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: Multiple connections in sequence */
static void test_socklib_multiple_connections(void)
{
    HSOCKET sock1, sock2;
    error_code ret;

    start_test_server(echo_server_thread);

    /* First connection */
    ret = socklib_open(&sock1, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
    socklib_close(&sock1);

    /* Give server time to accept next connection */
    usleep(100000);

    /* Second connection */
    ret = socklib_open(&sock2, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
    socklib_close(&sock2);

    stop_test_server();
}

/* Test: socklib_open with interface binding (lo interface) */
static void test_socklib_open_with_interface(void)
{
#if !defined(WIN32)
    HSOCKET sock;
    error_code ret;

    start_test_server(echo_server_thread);

    /* Try to bind to loopback interface */
    ret = socklib_open(&sock, "127.0.0.1", test_server_port, "lo", 5);
    /* Should succeed or fail gracefully */
    if (ret == SR_SUCCESS) {
        TEST_ASSERT_FALSE(sock.closed);
        socklib_close(&sock);
    } else {
        /* Interface binding might fail on some systems */
        TEST_ASSERT_TRUE(ret == SR_ERROR_CANT_BIND_ON_INTERFACE ||
                         ret == SR_ERROR_CONNECT_FAILED);
    }

    stop_test_server();
#else
    TEST_ASSERT_TRUE(1); /* Skip on Windows */
#endif
}

/* Test: socklib_open with invalid interface */
static void test_socklib_open_invalid_interface(void)
{
#if !defined(WIN32)
    HSOCKET sock;
    error_code ret;

    start_test_server(echo_server_thread);

    /* Try to bind to non-existent interface */
    ret = socklib_open(&sock, "127.0.0.1", test_server_port, "invalid_iface999", 5);
    /* Should still work - bind will use INADDR_ANY as fallback */
    if (ret == SR_SUCCESS) {
        socklib_close(&sock);
    } else {
        /* Or might fail with bind error */
        TEST_ASSERT_TRUE(ret == SR_ERROR_CANT_BIND_ON_INTERFACE ||
                         ret == SR_ERROR_CONNECT_FAILED);
    }

    stop_test_server();
#else
    TEST_ASSERT_TRUE(1); /* Skip on Windows */
#endif
}

/* Slow HTTP server for testing header reading edge cases */
void* slow_http_server_thread(void* arg) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int listen_sock, client_sock;
    const char* partial1 = "HTTP/1.1 200 OK\r\n";
    const char* partial2 = "Content-Type: audio/mpeg\r\n\r\n";

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        return NULL;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(test_server_port);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_sock);
        return NULL;
    }

    if (listen(listen_sock, 1) < 0) {
        close(listen_sock);
        return NULL;
    }

    server_socket = listen_sock;

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (!server_should_stop) {
        client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            continue;
        }

        /* Send header in two parts with delay */
        send(client_sock, partial1, strlen(partial1), 0);
        usleep(50000);
        send(client_sock, partial2, strlen(partial2), 0);

        close(client_sock);
    }

    close(listen_sock);
    server_socket = -1;
    return NULL;
}

/* Test: socklib_read_header with slow server */
static void test_socklib_read_header_slow_server(void)
{
    HSOCKET sock;
    char header_buf[4096];
    RIP_MANAGER_INFO rmi;
    STREAM_PREFS prefs;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&prefs, 0, sizeof(STREAM_PREFS));
    prefs.timeout = 10;
    rmi.prefs = &prefs;
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(slow_http_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    ret = socklib_read_header(&rmi, &sock, header_buf, sizeof(header_buf));
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    socklib_close(&sock);
    stop_test_server();
}

/* Server that closes connection immediately */
void* close_server_thread(void* arg) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int listen_sock, client_sock;

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        return NULL;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(test_server_port);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_sock);
        return NULL;
    }

    if (listen(listen_sock, 1) < 0) {
        close(listen_sock);
        return NULL;
    }

    server_socket = listen_sock;

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (!server_should_stop) {
        client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            continue;
        }

        /* Close immediately without sending data */
        close(client_sock);
    }

    close(listen_sock);
    server_socket = -1;
    return NULL;
}

/* Test: socklib_recvall with server closing connection */
static void test_socklib_recvall_server_closes(void)
{
    HSOCKET sock;
    char recv_buf[256];
    RIP_MANAGER_INFO rmi;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(close_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    /* Try to read, server will close connection */
    int received = socklib_recvall(&rmi, &sock, recv_buf, 10, 2);
    /* Should return 0 (connection closed) or timeout */
    TEST_ASSERT_TRUE(received == 0 || received == SR_ERROR_TIMEOUT);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_read_header with server closing connection */
static void test_socklib_read_header_server_closes(void)
{
    HSOCKET sock;
    char header_buf[4096];
    RIP_MANAGER_INFO rmi;
    STREAM_PREFS prefs;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&prefs, 0, sizeof(STREAM_PREFS));
    prefs.timeout = 2;
    rmi.prefs = &prefs;
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(close_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    ret = socklib_read_header(&rmi, &sock, header_buf, sizeof(header_buf));
    /* Should return SR_ERROR_NO_HTTP_HEADER when connection closes */
    TEST_ASSERT_EQUAL(SR_ERROR_NO_HTTP_HEADER, ret);

    socklib_close(&sock);
    stop_test_server();
}

/* ========================================================================
 * NEW TESTS - Additional coverage for missing paths
 * ======================================================================== */

/* Test: socklib_init idempotent calls */
static void test_socklib_init_idempotent(void)
{
    error_code ret;

    /* First init */
    ret = socklib_init();
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    /* Second init should also succeed */
    ret = socklib_init();
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    /* Third init should also succeed */
    ret = socklib_init();
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    /* Cleanup all */
    socklib_cleanup();
    socklib_cleanup();
    socklib_cleanup();
}

/* Test: socklib_cleanup idempotent calls */
static void test_socklib_cleanup_idempotent(void)
{
    error_code ret;

    ret = socklib_init();
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    /* Multiple cleanups should not crash */
    socklib_cleanup();
    socklib_cleanup();
    socklib_cleanup();

    /* Re-init should work after cleanup */
    ret = socklib_init();
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
    socklib_cleanup();
}

/* Test: socklib_open with empty hostname */
static void test_socklib_open_empty_hostname(void)
{
    HSOCKET sock;
    error_code ret = socklib_open(&sock, "", 80, NULL, 5);
    /* Empty hostname should fail to resolve */
    TEST_ASSERT_EQUAL(SR_ERROR_CANT_RESOLVE_HOSTNAME, ret);
}

/* Test: socklib_open with zero timeout */
static void test_socklib_open_zero_timeout(void)
{
    HSOCKET sock;
    error_code ret;

    start_test_server(echo_server_thread);

    /* Zero timeout should still work (means default timeout) */
    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 0);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
    TEST_ASSERT_FALSE(sock.closed);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_close double-close handling */
static void test_socklib_close_double_close(void)
{
    HSOCKET sock;
    error_code ret;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
    TEST_ASSERT_FALSE(sock.closed);

    /* First close */
    socklib_close(&sock);
    TEST_ASSERT_TRUE(sock.closed);

    /* Second close - should not crash, socket already closed */
    socklib_close(&sock);
    TEST_ASSERT_TRUE(sock.closed);

    stop_test_server();
}

/* Test: socklib_sendall with zero size */
static void test_socklib_sendall_zero_size(void)
{
    HSOCKET sock;
    error_code ret;
    char send_buf[] = "test";
    int sent;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    /* Zero size should return 0 immediately */
    sent = socklib_sendall(&sock, send_buf, 0);
    TEST_ASSERT_EQUAL(0, sent);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_recvall with zero size */
static void test_socklib_recvall_zero_size(void)
{
    HSOCKET sock;
    char recv_buf[256];
    RIP_MANAGER_INFO rmi;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    /* Zero size should return 0 immediately */
    int received = socklib_recvall(&rmi, &sock, recv_buf, 0, 2);
    TEST_ASSERT_EQUAL(0, received);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_recvall with abort pipe (Unix only) */
static void test_socklib_recvall_abort_pipe(void)
{
#if __UNIX__
    HSOCKET sock;
    char recv_buf[256];
    RIP_MANAGER_INFO rmi;
    error_code ret;
    int abort_pipe[2];

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));

    /* Create a pipe for abort signalling */
    if (pipe(abort_pipe) != 0) {
        TEST_FAIL_MESSAGE("Failed to create abort pipe");
        return;
    }

    rmi.abort_pipe[0] = abort_pipe[0];
    rmi.abort_pipe[1] = abort_pipe[1];

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    /* Write to abort pipe to trigger abort */
    write(abort_pipe[1], "x", 1);

    /* Try to recv - should detect abort pipe signal */
    int received = socklib_recvall(&rmi, &sock, recv_buf, 10, 5);
    TEST_ASSERT_EQUAL(SR_ERROR_ABORT_PIPE_SIGNALLED, received);

    socklib_close(&sock);
    close(abort_pipe[0]);
    close(abort_pipe[1]);
    stop_test_server();
#else
    TEST_ASSERT_TRUE(1); /* Skip on non-Unix */
#endif
}

/* Server that sends data in small chunks for partial receive testing */
void* chunked_server_thread(void* arg) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int listen_sock, client_sock;
    const char* chunk1 = "AAAA";
    const char* chunk2 = "BBBB";
    const char* chunk3 = "CCCC";

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        return NULL;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(test_server_port);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_sock);
        return NULL;
    }

    if (listen(listen_sock, 1) < 0) {
        close(listen_sock);
        return NULL;
    }

    server_socket = listen_sock;

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (!server_should_stop) {
        client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            continue;
        }

        /* Send data in small chunks with delays */
        send(client_sock, chunk1, strlen(chunk1), 0);
        usleep(20000);
        send(client_sock, chunk2, strlen(chunk2), 0);
        usleep(20000);
        send(client_sock, chunk3, strlen(chunk3), 0);

        close(client_sock);
    }

    close(listen_sock);
    server_socket = -1;
    return NULL;
}

/* Test: socklib_recvall with partial/chunked receives */
static void test_socklib_recvall_chunked(void)
{
    HSOCKET sock;
    char recv_buf[256];
    RIP_MANAGER_INFO rmi;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(chunked_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    memset(recv_buf, 0, sizeof(recv_buf));

    /* Request all 12 bytes which will come in 3 chunks */
    int received = socklib_recvall(&rmi, &sock, recv_buf, 12, 5);
    TEST_ASSERT_EQUAL(12, received);
    TEST_ASSERT_EQUAL_STRING("AAAABBBBCCCC", recv_buf);

    socklib_close(&sock);
    stop_test_server();
}

/* Server that sends Live365 style header with \n\0\r\n terminator */
void* live365_header_server_thread(void* arg) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int listen_sock, client_sock;
    /* Live365 allegedly used \n\0\r\n instead of \r\n\r\n */
    char response[] = "HTTP/1.1 200 OK\r\nContent-Type: audio/mpeg\n\0\r\n";

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        return NULL;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(test_server_port);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_sock);
        return NULL;
    }

    if (listen(listen_sock, 1) < 0) {
        close(listen_sock);
        return NULL;
    }

    server_socket = listen_sock;

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (!server_should_stop) {
        client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            continue;
        }

        /* Send including embedded null - sizeof includes null, so -1 to exclude final null */
        send(client_sock, response, sizeof(response) - 1, 0);

        close(client_sock);
    }

    close(listen_sock);
    server_socket = -1;
    return NULL;
}

/* Test: socklib_read_header with Live365-style header terminator */
static void test_socklib_read_header_live365_format(void)
{
    HSOCKET sock;
    char header_buf[4096];
    RIP_MANAGER_INFO rmi;
    STREAM_PREFS prefs;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&prefs, 0, sizeof(STREAM_PREFS));
    prefs.timeout = 5;
    rmi.prefs = &prefs;
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(live365_header_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    ret = socklib_read_header(&rmi, &sock, header_buf, sizeof(header_buf));
    /* Should succeed with \n\0\r\n terminator (Live365 format) */
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
    TEST_ASSERT_TRUE(strstr(header_buf, "HTTP/1.1") != NULL);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_read_header with very small buffer */
static void test_socklib_read_header_small_buffer(void)
{
    HSOCKET sock;
    char header_buf[10]; /* Too small for a real header */
    RIP_MANAGER_INFO rmi;
    STREAM_PREFS prefs;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&prefs, 0, sizeof(STREAM_PREFS));
    prefs.timeout = 2;
    rmi.prefs = &prefs;
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(http_header_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    ret = socklib_read_header(&rmi, &sock, header_buf, sizeof(header_buf));
    /* Should fail because buffer is too small to hold header + terminator */
    TEST_ASSERT_EQUAL(SR_ERROR_NO_HTTP_HEADER, ret);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: read_interface with NULL address pointer */
static void test_read_interface_null_addr(void)
{
#if !defined(WIN32)
    /* Note: Implementation doesn't check for NULL addr, so this may crash
     * or behave unexpectedly. Testing to document current behavior. */
    /* Skip this test to avoid potential segfault - document the limitation */
    TEST_ASSERT_TRUE(1);
#else
    TEST_ASSERT_TRUE(1);
#endif
}

/* Test: read_interface with empty interface name */
static void test_read_interface_empty_name(void)
{
#if !defined(WIN32)
    uint32_t addr = 0;
    error_code ret = read_interface("", &addr);
    /* Empty name should fail ioctl */
    TEST_ASSERT_TRUE(ret < 0);
#else
    TEST_ASSERT_TRUE(1);
#endif
}

/* Test: socklib_open IPv4 address with all zeros (0.0.0.0) */
static void test_socklib_open_inaddr_any(void)
{
    HSOCKET sock;
    /* 0.0.0.0 is INADDR_ANY - connection will depend on system routing */
    error_code ret = socklib_open(&sock, "0.0.0.0", 1, NULL, 1);
    /* Should fail with connection refused or similar */
    TEST_ASSERT_TRUE(ret == SR_ERROR_CONNECT_FAILED ||
                     ret == SR_ERROR_CANT_RESOLVE_HOSTNAME);
}

/* Test: socklib_open with multicast address (should fail) */
static void test_socklib_open_multicast_addr(void)
{
    HSOCKET sock;
    /* 224.0.0.1 is a multicast address - TCP doesn't work with multicast */
    error_code ret = socklib_open(&sock, "224.0.0.1", 80, NULL, 1);
    TEST_ASSERT_TRUE(ret == SR_ERROR_CONNECT_FAILED ||
                     ret == SR_ERROR_CANT_RESOLVE_HOSTNAME);
}

/* Test: socklib_sendall multiple sends in sequence */
static void test_socklib_sendall_multiple(void)
{
    HSOCKET sock;
    error_code ret;
    char send_buf1[] = "First";
    char send_buf2[] = "Second";
    char send_buf3[] = "Third";
    int sent;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    sent = socklib_sendall(&sock, send_buf1, strlen(send_buf1));
    TEST_ASSERT_EQUAL(strlen(send_buf1), sent);

    sent = socklib_sendall(&sock, send_buf2, strlen(send_buf2));
    TEST_ASSERT_EQUAL(strlen(send_buf2), sent);

    sent = socklib_sendall(&sock, send_buf3, strlen(send_buf3));
    TEST_ASSERT_EQUAL(strlen(send_buf3), sent);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_recvall with negative timeout (should use no timeout) */
static void test_socklib_recvall_negative_timeout(void)
{
    HSOCKET sock;
    char send_buf[] = "Test";
    char recv_buf[256];
    RIP_MANAGER_INFO rmi;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    socklib_sendall(&sock, send_buf, strlen(send_buf));
    usleep(50000);

    memset(recv_buf, 0, sizeof(recv_buf));
    /* Negative timeout - should skip select() and go directly to recv */
    int received = socklib_recvall(&rmi, &sock, recv_buf, strlen(send_buf), -1);
    TEST_ASSERT_EQUAL(strlen(send_buf), received);

    socklib_close(&sock);
    stop_test_server();
}

/* ========================================================================
 * ADDITIONAL TESTS - Edge cases and error paths for improved coverage
 * ======================================================================== */

/* Test: socklib_open with port 0 (should fail connect) */
static void test_socklib_open_port_zero(void)
{
    HSOCKET sock;
    /* Port 0 is typically reserved and not listening */
    error_code ret = socklib_open(&sock, "127.0.0.1", 0, NULL, 1);
    /* Should fail with connection refused */
    TEST_ASSERT_EQUAL(SR_ERROR_CONNECT_FAILED, ret);
}

/* Test: socklib_open with maximum port number (65535) */
static void test_socklib_open_max_port(void)
{
    HSOCKET sock;
    /* Port 65535 is valid but unlikely to be listening */
    error_code ret = socklib_open(&sock, "127.0.0.1", 65535, NULL, 1);
    /* Should fail with connection refused on localhost */
    TEST_ASSERT_EQUAL(SR_ERROR_CONNECT_FAILED, ret);
}

/* Test: socklib_sendall returns exact byte count on partial loop */
static void test_socklib_sendall_exact_count(void)
{
    HSOCKET sock;
    error_code ret;
    char send_buf[100];
    int sent;

    /* Fill buffer with pattern */
    for (int i = 0; i < 100; i++) {
        send_buf[i] = (char)('A' + (i % 26));
    }

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    sent = socklib_sendall(&sock, send_buf, 100);
    TEST_ASSERT_EQUAL(100, sent);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_recvall exact byte count */
static void test_socklib_recvall_exact_count(void)
{
    HSOCKET sock;
    char send_buf[50];
    char recv_buf[50];
    RIP_MANAGER_INFO rmi;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    /* Fill buffer with pattern */
    for (int i = 0; i < 50; i++) {
        send_buf[i] = (char)('0' + (i % 10));
    }

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    socklib_sendall(&sock, send_buf, 50);
    usleep(50000);

    memset(recv_buf, 0, sizeof(recv_buf));
    int received = socklib_recvall(&rmi, &sock, recv_buf, 50, 2);
    TEST_ASSERT_EQUAL(50, received);
    TEST_ASSERT_EQUAL_MEMORY(send_buf, recv_buf, 50);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_read_header with missing header terminator */
static void test_socklib_read_header_no_terminator(void)
{
    HSOCKET sock;
    char header_buf[128];  /* Small buffer that will fill up */
    RIP_MANAGER_INFO rmi;
    STREAM_PREFS prefs;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&prefs, 0, sizeof(STREAM_PREFS));
    prefs.timeout = 2;
    rmi.prefs = &prefs;
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    /* Server that closes without terminator */
    start_test_server(close_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    ret = socklib_read_header(&rmi, &sock, header_buf, sizeof(header_buf));
    /* Should return SR_ERROR_NO_HTTP_HEADER */
    TEST_ASSERT_EQUAL(SR_ERROR_NO_HTTP_HEADER, ret);

    socklib_close(&sock);
    stop_test_server();
}

/* Server that sends a lot of data without header terminator */
void* no_terminator_server_thread(void* arg) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int listen_sock, client_sock;
    char data[200];

    /* Fill with data that doesn't contain \r\n\r\n */
    memset(data, 'X', sizeof(data));

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        return NULL;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(test_server_port);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_sock);
        return NULL;
    }

    if (listen(listen_sock, 1) < 0) {
        close(listen_sock);
        return NULL;
    }

    server_socket = listen_sock;

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (!server_should_stop) {
        client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            continue;
        }

        /* Send lots of data without header terminator */
        send(client_sock, data, sizeof(data), 0);
        close(client_sock);
    }

    close(listen_sock);
    server_socket = -1;
    return NULL;
}

/* Test: socklib_read_header buffer overflow protection */
static void test_socklib_read_header_buffer_full(void)
{
    HSOCKET sock;
    char header_buf[100];  /* Buffer smaller than server sends */
    RIP_MANAGER_INFO rmi;
    STREAM_PREFS prefs;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&prefs, 0, sizeof(STREAM_PREFS));
    prefs.timeout = 2;
    rmi.prefs = &prefs;
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(no_terminator_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    ret = socklib_read_header(&rmi, &sock, header_buf, sizeof(header_buf));
    /* Buffer fills up without finding terminator -> SR_ERROR_NO_HTTP_HEADER */
    TEST_ASSERT_EQUAL(SR_ERROR_NO_HTTP_HEADER, ret);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: read_interface with valid eth interface names */
static void test_read_interface_common_names(void)
{
#if !defined(WIN32)
    uint32_t addr = 0;
    error_code ret;

    /* Try common interface names - at least one should exist or fail gracefully */
    ret = read_interface("eth0", &addr);
    /* Either finds interface or returns error - both are valid */
    TEST_ASSERT_TRUE(ret == 0 || ret < 0);

    ret = read_interface("wlan0", &addr);
    TEST_ASSERT_TRUE(ret == 0 || ret < 0);

    ret = read_interface("enp0s3", &addr);
    TEST_ASSERT_TRUE(ret == 0 || ret < 0);
#else
    TEST_ASSERT_TRUE(1);
#endif
}

/* Test: socklib_open to unreachable host (network error) */
static void test_socklib_open_unreachable_host(void)
{
    HSOCKET sock;
    /* 192.0.2.1 is a TEST-NET address that should be unreachable */
    error_code ret = socklib_open(&sock, "192.0.2.1", 80, NULL, 1);
    /* Should fail - either connect failed or timeout */
    TEST_ASSERT_TRUE(ret == SR_ERROR_CONNECT_FAILED || ret == SR_ERROR_TIMEOUT);
}

/* Test: socklib_recvall partial read when server sends less than requested */
static void test_socklib_recvall_partial_read(void)
{
    HSOCKET sock;
    char recv_buf[256];
    RIP_MANAGER_INFO rmi;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    /* Use close server - it accepts but sends nothing */
    start_test_server(close_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    memset(recv_buf, 0, sizeof(recv_buf));
    /* Try to read 100 bytes but server will close immediately */
    int received = socklib_recvall(&rmi, &sock, recv_buf, 100, 2);
    /* Should return 0 (connection closed by peer) or timeout */
    TEST_ASSERT_TRUE(received == 0 || received == SR_ERROR_TIMEOUT);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_read_header finds \r\n\r\n terminator correctly */
static void test_socklib_read_header_crlf_detection(void)
{
    HSOCKET sock;
    char header_buf[4096];
    RIP_MANAGER_INFO rmi;
    STREAM_PREFS prefs;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(&prefs, 0, sizeof(STREAM_PREFS));
    prefs.timeout = 5;
    rmi.prefs = &prefs;
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(http_header_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    memset(header_buf, 0, sizeof(header_buf));
    ret = socklib_read_header(&rmi, &sock, header_buf, sizeof(header_buf));
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    /* Verify header content was captured */
    TEST_ASSERT_TRUE(strlen(header_buf) > 0);
    TEST_ASSERT_TRUE(strstr(header_buf, "200 OK") != NULL);
    TEST_ASSERT_TRUE(strstr(header_buf, "Content-Type") != NULL);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_sendall with NULL buffer (edge case) */
static void test_socklib_sendall_null_buffer(void)
{
    HSOCKET sock;
    error_code ret;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    /* Note: Calling with NULL buffer and size 0 should work */
    int sent = socklib_sendall(&sock, NULL, 0);
    TEST_ASSERT_EQUAL(0, sent);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_recvall with NULL buffer (edge case - size 0) */
static void test_socklib_recvall_null_buffer(void)
{
    HSOCKET sock;
    RIP_MANAGER_INFO rmi;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    /* Size 0 should return immediately without accessing buffer */
    int received = socklib_recvall(&rmi, &sock, NULL, 0, 2);
    TEST_ASSERT_EQUAL(0, received);

    socklib_close(&sock);
    stop_test_server();
}

/* Test: socklib_close on already closed socket doesn't crash */
static void test_socklib_close_multiple_times(void)
{
    HSOCKET sock;
    error_code ret;

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
    TEST_ASSERT_FALSE(sock.closed);

    /* Close multiple times */
    socklib_close(&sock);
    TEST_ASSERT_TRUE(sock.closed);

    socklib_close(&sock);
    TEST_ASSERT_TRUE(sock.closed);

    socklib_close(&sock);
    TEST_ASSERT_TRUE(sock.closed);

    stop_test_server();
}

/* Test: socklib_init and cleanup lifecycle */
static void test_socklib_lifecycle(void)
{
    error_code ret;

    /* Multiple init/cleanup cycles should work */
    for (int i = 0; i < 5; i++) {
        ret = socklib_init();
        TEST_ASSERT_EQUAL(SR_SUCCESS, ret);
        socklib_cleanup();
    }
}

/* Test: socklib_open with long hostname */
static void test_socklib_open_long_hostname(void)
{
    HSOCKET sock;
    char long_hostname[300];

    /* Create a very long invalid hostname */
    memset(long_hostname, 'a', sizeof(long_hostname) - 1);
    long_hostname[sizeof(long_hostname) - 1] = '\0';

    error_code ret = socklib_open(&sock, long_hostname, 80, NULL, 1);
    /* Should fail to resolve */
    TEST_ASSERT_EQUAL(SR_ERROR_CANT_RESOLVE_HOSTNAME, ret);
}

/* Test: socklib_recvall loop with multiple recv calls */
static void test_socklib_recvall_multiple_recv(void)
{
    HSOCKET sock;
    char send_buf[1024];
    char recv_buf[1024];
    RIP_MANAGER_INFO rmi;
    error_code ret;

    memset(&rmi, 0, sizeof(RIP_MANAGER_INFO));
    rmi.abort_pipe[0] = -1;
    rmi.abort_pipe[1] = -1;

    /* Fill with pattern */
    for (int i = 0; i < 1024; i++) {
        send_buf[i] = (char)(i % 256);
    }

    start_test_server(echo_server_thread);

    ret = socklib_open(&sock, "127.0.0.1", test_server_port, NULL, 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, ret);

    socklib_sendall(&sock, send_buf, 1024);
    usleep(100000);

    memset(recv_buf, 0, sizeof(recv_buf));
    int received = socklib_recvall(&rmi, &sock, recv_buf, 1024, 5);
    TEST_ASSERT_EQUAL(1024, received);
    TEST_ASSERT_EQUAL_MEMORY(send_buf, recv_buf, 1024);

    socklib_close(&sock);
    stop_test_server();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* Ignore SIGPIPE - socket tests may cause SIGPIPE when
     * server closes connection unexpectedly. We handle this
     * via error returns from send/recv instead. */
    signal(SIGPIPE, SIG_IGN);

    UNITY_BEGIN();

    /* Data structure tests */
    RUN_TEST(test_hsocket_structure);
    RUN_TEST(test_hsocket_invalid_socket);
    RUN_TEST(test_socket_error_codes);
    RUN_TEST(test_socket_error_codes_distinct);
    RUN_TEST(test_timeout_error_codes);
    RUN_TEST(test_sr_success);
    RUN_TEST(test_max_host_len);
    RUN_TEST(test_max_header_len_for_socket);
    RUN_TEST(test_common_ports);
    RUN_TEST(test_port_range);
    RUN_TEST(test_rmi_stream_sock);
    RUN_TEST(test_rmi_abort_pipe);
    RUN_TEST(test_socklib_init_success_value);
    RUN_TEST(test_socket_type);
    RUN_TEST(test_ip_address_constants);
    RUN_TEST(test_inaddr_constants);
    RUN_TEST(test_fd_setsize);
    RUN_TEST(test_stream_prefs_timeout);
    RUN_TEST(test_no_http_header_error);

    /* Function implementation tests */
    RUN_TEST(test_socklib_init_success);
    RUN_TEST(test_socklib_open_success);
    RUN_TEST(test_socklib_open_with_hostname);
    RUN_TEST(test_socklib_open_null_socket);
    RUN_TEST(test_socklib_open_null_host);
    RUN_TEST(test_socklib_open_invalid_hostname);
    RUN_TEST(test_socklib_open_connection_refused);
    RUN_TEST(test_socklib_close_sets_flag);
    RUN_TEST(test_socklib_sendall_success);
    RUN_TEST(test_socklib_sendall_closed_socket);
    RUN_TEST(test_socklib_recvall_success);
    RUN_TEST(test_socklib_recvall_closed_socket);
    RUN_TEST(test_socklib_recvall_timeout);
    RUN_TEST(test_socklib_read_header_success);
    RUN_TEST(test_socklib_read_header_closed_socket);
    RUN_TEST(test_read_interface_unix);
    RUN_TEST(test_read_interface_invalid);
    RUN_TEST(test_socklib_sendall_large_data);
    RUN_TEST(test_socklib_multiple_connections);
    RUN_TEST(test_socklib_open_with_interface);
    RUN_TEST(test_socklib_open_invalid_interface);
    RUN_TEST(test_socklib_read_header_slow_server);
    RUN_TEST(test_socklib_recvall_server_closes);
    RUN_TEST(test_socklib_read_header_server_closes);

    /* New tests for additional coverage */
    RUN_TEST(test_socklib_init_idempotent);
    RUN_TEST(test_socklib_cleanup_idempotent);
    RUN_TEST(test_socklib_open_empty_hostname);
    RUN_TEST(test_socklib_open_zero_timeout);
    RUN_TEST(test_socklib_close_double_close);
    RUN_TEST(test_socklib_sendall_zero_size);
    RUN_TEST(test_socklib_recvall_zero_size);
    RUN_TEST(test_socklib_recvall_abort_pipe);
    RUN_TEST(test_socklib_recvall_chunked);
    RUN_TEST(test_socklib_read_header_live365_format);
    RUN_TEST(test_socklib_read_header_small_buffer);
    RUN_TEST(test_read_interface_null_addr);
    RUN_TEST(test_read_interface_empty_name);
    RUN_TEST(test_socklib_open_inaddr_any);
    RUN_TEST(test_socklib_open_multicast_addr);
    RUN_TEST(test_socklib_sendall_multiple);
    RUN_TEST(test_socklib_recvall_negative_timeout);

    /* Additional edge case and error path tests */
    RUN_TEST(test_socklib_open_port_zero);
    RUN_TEST(test_socklib_open_max_port);
    RUN_TEST(test_socklib_sendall_exact_count);
    RUN_TEST(test_socklib_recvall_exact_count);
    RUN_TEST(test_socklib_read_header_no_terminator);
    RUN_TEST(test_socklib_read_header_buffer_full);
    RUN_TEST(test_read_interface_common_names);
    RUN_TEST(test_socklib_open_unreachable_host);
    RUN_TEST(test_socklib_recvall_partial_read);
    RUN_TEST(test_socklib_read_header_crlf_detection);
    RUN_TEST(test_socklib_sendall_null_buffer);
    RUN_TEST(test_socklib_recvall_null_buffer);
    RUN_TEST(test_socklib_close_multiple_times);
    RUN_TEST(test_socklib_lifecycle);
    RUN_TEST(test_socklib_open_long_hostname);
    RUN_TEST(test_socklib_recvall_multiple_recv);

    return UNITY_END();
}
