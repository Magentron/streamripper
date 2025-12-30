/* test_e2e_reconnect.c
 *
 * E2E tests for reconnection configuration and retry settings.
 *
 * These tests focus on:
 * - Auto-reconnect configuration
 * - Timeout settings
 * - Connection retry behavior configuration
 * - Socket and connection settings
 *
 * Note: Full reconnection tests require the mock server with drop capability.
 * These tests validate the reconnection infrastructure and settings.
 */

#include <string.h>
#include <unity.h>

#include "errors.h"
#include "http.h"
#include "rip_manager.h"
#include "srtypes.h"

#include "test_e2e_helpers.h"

/* Test fixtures */
static STREAM_PREFS test_prefs;
static char temp_dir[256];

/* ========================================================================== */
/* Setup and Teardown */
/* ========================================================================== */

void setUp(void)
{
    /* Create temp directory */
    if (!create_temp_directory(temp_dir, sizeof(temp_dir), "e2e_reconnect")) {
        strcpy(temp_dir, "/tmp/e2e_reconnect_test");
    }

    /* Initialize prefs */
    memset(&test_prefs, 0, sizeof(test_prefs));
    set_rip_manager_options_defaults(&test_prefs);
}

void tearDown(void)
{
    remove_directory_recursive(temp_dir);
}

/* ========================================================================== */
/* Tests: Auto-reconnect flag */
/* ========================================================================== */

static void test_auto_reconnect_default_enabled(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Auto-reconnect should be ON by default */
    TEST_ASSERT_TRUE(GET_AUTO_RECONNECT(prefs.flags));
}

static void test_auto_reconnect_disable(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Disable auto-reconnect */
    OPT_FLAG_SET(prefs.flags, OPT_AUTO_RECONNECT, 0);
    TEST_ASSERT_FALSE(GET_AUTO_RECONNECT(prefs.flags));
}

static void test_auto_reconnect_enable(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Start with disabled */
    OPT_FLAG_SET(prefs.flags, OPT_AUTO_RECONNECT, 0);
    TEST_ASSERT_FALSE(GET_AUTO_RECONNECT(prefs.flags));

    /* Re-enable */
    OPT_FLAG_SET(prefs.flags, OPT_AUTO_RECONNECT, 1);
    TEST_ASSERT_TRUE(GET_AUTO_RECONNECT(prefs.flags));
}

static void test_auto_reconnect_flag_independence(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Changing auto-reconnect shouldn't affect other flags */
    int other_flag_before = GET_SEPARATE_DIRS(prefs.flags);

    OPT_FLAG_SET(prefs.flags, OPT_AUTO_RECONNECT, 0);
    OPT_FLAG_SET(prefs.flags, OPT_AUTO_RECONNECT, 1);

    TEST_ASSERT_EQUAL(other_flag_before, GET_SEPARATE_DIRS(prefs.flags));
}

/* ========================================================================== */
/* Tests: Timeout configuration */
/* ========================================================================== */

static void test_timeout_default_value(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default timeout should be reasonable (not 0, not too large) */
    /* Actual default might vary; we just check it's set */
    TEST_ASSERT_TRUE(prefs.timeout >= 0);
}

static void test_timeout_set_short(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Set short timeout for quick testing */
    prefs.timeout = 5;
    TEST_ASSERT_EQUAL(5, prefs.timeout);
}

static void test_timeout_set_long(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Set longer timeout for slow connections */
    prefs.timeout = 120;
    TEST_ASSERT_EQUAL(120, prefs.timeout);
}

static void test_timeout_set_zero(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Zero timeout might mean "no timeout" or use default */
    prefs.timeout = 0;
    TEST_ASSERT_EQUAL(0, prefs.timeout);
}

static void test_timeout_type_is_ulong(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Verify timeout can hold large values */
    prefs.timeout = 3600;  /* 1 hour */
    TEST_ASSERT_EQUAL(3600, prefs.timeout);

    prefs.timeout = 86400; /* 24 hours */
    TEST_ASSERT_EQUAL(86400, prefs.timeout);
}

/* ========================================================================== */
/* Tests: Relay port settings */
/* ========================================================================== */

static void test_relay_port_default(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default relay port is 8000 */
    TEST_ASSERT_EQUAL(8000, prefs.relay_port);
}

static void test_relay_port_custom(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    prefs.relay_port = 9000;
    TEST_ASSERT_EQUAL(9000, prefs.relay_port);
}

static void test_max_port_default(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default max port is 18000 */
    TEST_ASSERT_EQUAL(18000, prefs.max_port);
}

static void test_max_port_custom(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    prefs.max_port = 20000;
    TEST_ASSERT_EQUAL(20000, prefs.max_port);
}

static void test_search_ports_flag(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Search ports should be ON by default */
    TEST_ASSERT_TRUE(GET_SEARCH_PORTS(prefs.flags));

    /* Disable */
    OPT_FLAG_SET(prefs.flags, OPT_SEARCH_PORTS, 0);
    TEST_ASSERT_FALSE(GET_SEARCH_PORTS(prefs.flags));
}

/* ========================================================================== */
/* Tests: Interface binding */
/* ========================================================================== */

static void test_if_name_default_empty(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default is no specific interface (empty string) */
    TEST_ASSERT_EQUAL_STRING("", prefs.if_name);
}

static void test_if_name_set_interface(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Bind to specific interface */
    strncpy(prefs.if_name, "eth0", sizeof(prefs.if_name) - 1);
    TEST_ASSERT_EQUAL_STRING("eth0", prefs.if_name);
}

static void test_if_name_set_ip_address(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Can also bind by IP address */
    strncpy(prefs.if_name, "192.168.1.100", sizeof(prefs.if_name) - 1);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", prefs.if_name);
}

/* ========================================================================== */
/* Tests: Relay IP binding */
/* ========================================================================== */

static void test_relay_ip_default_empty(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default is any interface (empty string) */
    TEST_ASSERT_EQUAL_STRING("", prefs.relay_ip);
}

static void test_relay_ip_set_localhost(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Bind relay to localhost only */
    strncpy(prefs.relay_ip, "127.0.0.1", sizeof(prefs.relay_ip) - 1);
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", prefs.relay_ip);
}

static void test_relay_ip_set_all_interfaces(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Bind to all interfaces */
    strncpy(prefs.relay_ip, "0.0.0.0", sizeof(prefs.relay_ip) - 1);
    TEST_ASSERT_EQUAL_STRING("0.0.0.0", prefs.relay_ip);
}

/* ========================================================================== */
/* Tests: Max connections */
/* ========================================================================== */

static void test_max_connections_default(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default max connections is 1 */
    TEST_ASSERT_EQUAL(1, prefs.max_connections);
}

static void test_max_connections_increase(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Allow more relay connections */
    prefs.max_connections = 10;
    TEST_ASSERT_EQUAL(10, prefs.max_connections);
}

static void test_max_connections_unlimited(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Zero might mean unlimited */
    prefs.max_connections = 0;
    TEST_ASSERT_EQUAL(0, prefs.max_connections);
}

/* ========================================================================== */
/* Tests: Make relay flag */
/* ========================================================================== */

static void test_make_relay_default(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Check default value of make relay flag */
    int make_relay = GET_MAKE_RELAY(prefs.flags);
    /* Just verify it's a boolean */
    TEST_ASSERT_TRUE(make_relay == 0 || make_relay == 1);
}

static void test_make_relay_enable(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    OPT_FLAG_SET(prefs.flags, OPT_MAKE_RELAY, 1);
    TEST_ASSERT_TRUE(GET_MAKE_RELAY(prefs.flags));
}

static void test_make_relay_disable(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    OPT_FLAG_SET(prefs.flags, OPT_MAKE_RELAY, 0);
    TEST_ASSERT_FALSE(GET_MAKE_RELAY(prefs.flags));
}

/* ========================================================================== */
/* Tests: User agent */
/* ========================================================================== */

static void test_useragent_default(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default useragent should be set to something */
    TEST_ASSERT_TRUE(strlen(prefs.useragent) > 0);
}

static void test_useragent_custom(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Set custom user agent */
    strncpy(prefs.useragent, "Custom Agent/1.0", sizeof(prefs.useragent) - 1);
    TEST_ASSERT_EQUAL_STRING("Custom Agent/1.0", prefs.useragent);
}

static void test_useragent_max_length(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Verify MAX_USERAGENT_STR is defined */
    TEST_ASSERT_TRUE(MAX_USERAGENT_STR > 0);

    /* Create a long useragent */
    char long_ua[MAX_USERAGENT_STR];
    memset(long_ua, 'A', MAX_USERAGENT_STR - 1);
    long_ua[MAX_USERAGENT_STR - 1] = '\0';

    strncpy(prefs.useragent, long_ua, sizeof(prefs.useragent) - 1);
    prefs.useragent[sizeof(prefs.useragent) - 1] = '\0';

    /* Should not crash */
    TEST_ASSERT_TRUE(strlen(prefs.useragent) > 0);
}

/* ========================================================================== */
/* Tests: HTTP 1.0 mode */
/* ========================================================================== */

static void test_http10_default(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* HTTP/1.0 mode should be OFF by default (use 1.1) */
    TEST_ASSERT_EQUAL(0, prefs.http10);
}

static void test_http10_enable(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Enable HTTP/1.0 mode for compatibility with old servers */
    prefs.http10 = 1;
    TEST_ASSERT_EQUAL(1, prefs.http10);
}

/* ========================================================================== */
/* Tests: Proxy configuration */
/* ========================================================================== */

static void test_proxy_default_empty(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    /* Default is no proxy */
    TEST_ASSERT_EQUAL_STRING("", prefs.proxyurl);
}

static void test_proxy_http(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    strncpy(prefs.proxyurl, "http://proxy.example.com:8080", sizeof(prefs.proxyurl) - 1);
    TEST_ASSERT_EQUAL_STRING("http://proxy.example.com:8080", prefs.proxyurl);
}

static void test_proxy_with_auth(void)
{
    STREAM_PREFS prefs;
    set_rip_manager_options_defaults(&prefs);

    strncpy(prefs.proxyurl, "http://user:pass@proxy.example.com:8080",
            sizeof(prefs.proxyurl) - 1);
    TEST_ASSERT_EQUAL_STRING("http://user:pass@proxy.example.com:8080", prefs.proxyurl);
}

/* ========================================================================== */
/* Tests: RIP_MANAGER_INFO status values */
/* ========================================================================== */

static void test_rm_status_buffering(void)
{
    /* Verify status constants */
    TEST_ASSERT_EQUAL(0x01, RM_STATUS_BUFFERING);
}

static void test_rm_status_ripping(void)
{
    TEST_ASSERT_EQUAL(0x02, RM_STATUS_RIPPING);
}

static void test_rm_status_reconnecting(void)
{
    TEST_ASSERT_EQUAL(0x03, RM_STATUS_RECONNECTING);
}

/* ========================================================================== */
/* Tests: Callback message types */
/* ========================================================================== */

static void test_rm_callback_update(void)
{
    TEST_ASSERT_EQUAL(0x01, RM_UPDATE);
}

static void test_rm_callback_error(void)
{
    TEST_ASSERT_EQUAL(0x02, RM_ERROR);
}

static void test_rm_callback_done(void)
{
    TEST_ASSERT_EQUAL(0x03, RM_DONE);
}

static void test_rm_callback_started(void)
{
    TEST_ASSERT_EQUAL(0x04, RM_STARTED);
}

static void test_rm_callback_new_track(void)
{
    TEST_ASSERT_EQUAL(0x05, RM_NEW_TRACK);
}

static void test_rm_callback_track_done(void)
{
    TEST_ASSERT_EQUAL(0x06, RM_TRACK_DONE);
}

/* ========================================================================== */
/* Tests: HSOCKET structure */
/* ========================================================================== */

static void test_hsocket_init(void)
{
    HSOCKET sock;
    memset(&sock, 0, sizeof(sock));

    sock.s = -1;  /* Invalid socket */
    sock.closed = TRUE;

    TEST_ASSERT_EQUAL(-1, sock.s);
    TEST_ASSERT_TRUE(sock.closed);
}

static void test_hsocket_open_state(void)
{
    HSOCKET sock;
    memset(&sock, 0, sizeof(sock));

    /* Simulate open socket */
    sock.s = 5;  /* Some valid fd */
    sock.closed = FALSE;

    TEST_ASSERT_TRUE(sock.s > 0);
    TEST_ASSERT_FALSE(sock.closed);
}

/* ========================================================================== */
/* Tests: E2E helper functions */
/* ========================================================================== */

static void test_e2e_helper_init_stream_prefs(void)
{
    STREAM_PREFS prefs;

    e2e_init_stream_prefs(&prefs, "http://test.example.com:8000/stream", temp_dir);

    TEST_ASSERT_EQUAL_STRING("http://test.example.com:8000/stream", prefs.url);
    TEST_ASSERT_EQUAL_STRING(temp_dir, prefs.output_directory);
}

static void test_e2e_helper_set_reconnect_options(void)
{
    STREAM_PREFS prefs;
    e2e_init_stream_prefs(&prefs, "http://test.example.com/stream", temp_dir);

    /* Enable reconnect with 30 second timeout */
    e2e_set_reconnect_options(&prefs, true, 30);

    TEST_ASSERT_TRUE(GET_AUTO_RECONNECT(prefs.flags));
    TEST_ASSERT_EQUAL(30, prefs.timeout);

    /* Disable reconnect */
    e2e_set_reconnect_options(&prefs, false, 60);

    TEST_ASSERT_FALSE(GET_AUTO_RECONNECT(prefs.flags));
    TEST_ASSERT_EQUAL(60, prefs.timeout);
}

/* ========================================================================== */
/* Main */
/* ========================================================================== */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    /* Auto-reconnect flag tests */
    RUN_TEST(test_auto_reconnect_default_enabled);
    RUN_TEST(test_auto_reconnect_disable);
    RUN_TEST(test_auto_reconnect_enable);
    RUN_TEST(test_auto_reconnect_flag_independence);

    /* Timeout configuration tests */
    RUN_TEST(test_timeout_default_value);
    RUN_TEST(test_timeout_set_short);
    RUN_TEST(test_timeout_set_long);
    RUN_TEST(test_timeout_set_zero);
    RUN_TEST(test_timeout_type_is_ulong);

    /* Relay port tests */
    RUN_TEST(test_relay_port_default);
    RUN_TEST(test_relay_port_custom);
    RUN_TEST(test_max_port_default);
    RUN_TEST(test_max_port_custom);
    RUN_TEST(test_search_ports_flag);

    /* Interface binding tests */
    RUN_TEST(test_if_name_default_empty);
    RUN_TEST(test_if_name_set_interface);
    RUN_TEST(test_if_name_set_ip_address);

    /* Relay IP tests */
    RUN_TEST(test_relay_ip_default_empty);
    RUN_TEST(test_relay_ip_set_localhost);
    RUN_TEST(test_relay_ip_set_all_interfaces);

    /* Max connections tests */
    RUN_TEST(test_max_connections_default);
    RUN_TEST(test_max_connections_increase);
    RUN_TEST(test_max_connections_unlimited);

    /* Make relay tests */
    RUN_TEST(test_make_relay_default);
    RUN_TEST(test_make_relay_enable);
    RUN_TEST(test_make_relay_disable);

    /* User agent tests */
    RUN_TEST(test_useragent_default);
    RUN_TEST(test_useragent_custom);
    RUN_TEST(test_useragent_max_length);

    /* HTTP 1.0 mode tests */
    RUN_TEST(test_http10_default);
    RUN_TEST(test_http10_enable);

    /* Proxy tests */
    RUN_TEST(test_proxy_default_empty);
    RUN_TEST(test_proxy_http);
    RUN_TEST(test_proxy_with_auth);

    /* RIP_MANAGER status tests */
    RUN_TEST(test_rm_status_buffering);
    RUN_TEST(test_rm_status_ripping);
    RUN_TEST(test_rm_status_reconnecting);

    /* Callback message type tests */
    RUN_TEST(test_rm_callback_update);
    RUN_TEST(test_rm_callback_error);
    RUN_TEST(test_rm_callback_done);
    RUN_TEST(test_rm_callback_started);
    RUN_TEST(test_rm_callback_new_track);
    RUN_TEST(test_rm_callback_track_done);

    /* HSOCKET tests */
    RUN_TEST(test_hsocket_init);
    RUN_TEST(test_hsocket_open_state);

    /* E2E helper tests */
    RUN_TEST(test_e2e_helper_init_stream_prefs);
    RUN_TEST(test_e2e_helper_set_reconnect_options);

    return UNITY_END();
}
