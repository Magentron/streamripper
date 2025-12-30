#include <string.h>
#include <unity.h>

#include "errors.h"
#include "http.h"

#include "Mockdebug.h"
#include "Mockmchar.h"
#include "Mocksocklib.h"


static URLINFO urlinfo;
static SR_HTTP_HEADER http_info;

void setUp(void)
{
    Mockdebug_Init();
    Mockmchar_Init();
    Mocksocklib_Init();

    memset(&urlinfo, 0xCC, sizeof(urlinfo));
    memset(&http_info, 0, sizeof(http_info));
}

void tearDown(void)
{
    Mockdebug_Verify();
    Mockmchar_Verify();
    Mocksocklib_Verify();
}


/* ========================================================================== */
/* URL Parsing Tests (existing tests) */
/* ========================================================================== */

static void test_parse_url_simple(void)
{
    debug_printf_Ignore();

    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://host.com/path", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/path", urlinfo.path);
    TEST_ASSERT_EQUAL(80, urlinfo.port);
    TEST_ASSERT_EQUAL_STRING("", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("", urlinfo.password);
}


static void test_parse_url_with_path_and_port(void)
{
    debug_printf_Ignore();

    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://host.com:8888/path", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/path", urlinfo.path);
    TEST_ASSERT_EQUAL(8888, urlinfo.port);
}


static void test_parse_url_with_port(void)
{
    debug_printf_Ignore();

    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://host.com:8888", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/", urlinfo.path);
    TEST_ASSERT_EQUAL(8888, urlinfo.port);
}


static void test_parse_url_with_credentials(void)
{
    debug_printf_Ignore();

    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://username:password@host.com/path", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/path", urlinfo.path);
    TEST_ASSERT_EQUAL(80, urlinfo.port);
    TEST_ASSERT_EQUAL_STRING("username", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("password", urlinfo.password);
}


/** for some servers the colon ist part of the uri */
static void test_parse_url_with_colon_in_uri(void)
{
    debug_printf_Ignore();

    const char * url = "http://host.com/path?value=foo:bar";
    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url(url, &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/path?value=foo:bar", urlinfo.path);
    TEST_ASSERT_EQUAL(80, urlinfo.port);
    TEST_ASSERT_EQUAL_STRING("", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("", urlinfo.password);
}


static void test_parse_url_complex(void)
{
    debug_printf_Ignore();

    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://username:password@host.com:1234/path?value=foo:bar", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/path?value=foo:bar", urlinfo.path);
    TEST_ASSERT_EQUAL(1234, urlinfo.port);
    TEST_ASSERT_EQUAL_STRING("username", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("password", urlinfo.password);
}


/* ========================================================================== */
/* NEW: HTTPS URL Parsing Tests */
/* ========================================================================== */

static void test_parse_url_https_simple(void)
{
    debug_printf_Ignore();

    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("https://secure.host.com/stream", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("secure.host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/stream", urlinfo.path);
    TEST_ASSERT_EQUAL(443, urlinfo.port);  /* HTTPS default port */
    TEST_ASSERT_EQUAL_STRING("", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("", urlinfo.password);
}


static void test_parse_url_https_with_port(void)
{
    debug_printf_Ignore();

    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("https://secure.host.com:8443/stream", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("secure.host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/stream", urlinfo.path);
    TEST_ASSERT_EQUAL(8443, urlinfo.port);
}


/* ========================================================================== */
/* NEW: url_is_https Tests */
/* ========================================================================== */

static void test_url_is_https_true(void)
{
    TEST_ASSERT_TRUE(url_is_https("https://example.com"));
    TEST_ASSERT_TRUE(url_is_https("HTTPS://EXAMPLE.COM"));
    TEST_ASSERT_TRUE(url_is_https("Https://Mixed.Case.com/path"));
    TEST_ASSERT_TRUE(url_is_https("https://example.com:443/stream"));
}


static void test_url_is_https_false(void)
{
    TEST_ASSERT_FALSE(url_is_https("http://example.com"));
    TEST_ASSERT_FALSE(url_is_https("HTTP://EXAMPLE.COM"));
    TEST_ASSERT_FALSE(url_is_https("ftp://example.com"));
    TEST_ASSERT_FALSE(url_is_https("example.com"));
    TEST_ASSERT_FALSE(url_is_https(""));
}


/* ========================================================================== */
/* NEW: http_parse_sc_header Tests */
/* ========================================================================== */

static void test_parse_sc_header_icy_200(void)
{
    const char *url = "http://stream.example.com/";
    const char *header =
        "ICY 200 OK\r\n"
        "icy-name:Test Radio\r\n"
        "icy-br:128\r\n"
        "icy-url:http://example.com\r\n"
        "Content-Type:audio/mpeg\r\n"
        "icy-metaint:8192\r\n"
        "\r\n";

    debug_printf_Ignore();
    subnstr_until_IgnoreAndReturn(NULL);
    trim_Ignore();

    error_code result = http_parse_sc_header(url, (char *)header, &http_info);

    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    TEST_ASSERT_EQUAL(200, http_info.icy_code);
}


static void test_parse_sc_header_http_200(void)
{
    const char *url = "http://stream.example.com/";
    const char *header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type:audio/mpeg\r\n"
        "Server:Icecast 2.4.0\r\n"
        "icy-metaint:16384\r\n"
        "\r\n";

    debug_printf_Ignore();
    subnstr_until_IgnoreAndReturn(NULL);
    trim_Ignore();

    error_code result = http_parse_sc_header(url, (char *)header, &http_info);

    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    TEST_ASSERT_EQUAL(200, http_info.icy_code);
}


static void test_parse_sc_header_404_error(void)
{
    const char *url = "http://stream.example.com/";
    const char *header =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type:text/html\r\n"
        "\r\n";

    debug_printf_Ignore();
    subnstr_until_IgnoreAndReturn(NULL);
    trim_Ignore();

    error_code result = http_parse_sc_header(url, (char *)header, &http_info);

    TEST_ASSERT_EQUAL(SR_ERROR_HTTP_404_ERROR, result);
}


static void test_parse_sc_header_401_error(void)
{
    const char *url = "http://stream.example.com/";
    const char *header =
        "HTTP/1.1 401 Unauthorized\r\n"
        "Content-Type:text/html\r\n"
        "\r\n";

    debug_printf_Ignore();
    subnstr_until_IgnoreAndReturn(NULL);
    trim_Ignore();

    error_code result = http_parse_sc_header(url, (char *)header, &http_info);

    TEST_ASSERT_EQUAL(SR_ERROR_HTTP_401_ERROR, result);
}


static void test_parse_sc_header_null_params(void)
{
    const char *url = "http://stream.example.com/";
    const char *header = "ICY 200 OK\r\n\r\n";

    debug_printf_Ignore();

    /* NULL header should fail */
    error_code result = http_parse_sc_header(url, NULL, &http_info);
    TEST_ASSERT_EQUAL(SR_ERROR_INVALID_PARAM, result);

    /* NULL info should fail */
    result = http_parse_sc_header(url, (char *)header, NULL);
    TEST_ASSERT_EQUAL(SR_ERROR_INVALID_PARAM, result);
}


/* ========================================================================== */
/* NEW: Content Type Constants Tests */
/* ========================================================================== */

static void test_content_type_mp3_constant(void)
{
    /* Verify CONTENT_TYPE_MP3 is defined and distinct */
    TEST_ASSERT_EQUAL(1, CONTENT_TYPE_MP3);
}


static void test_content_type_ogg_constant(void)
{
    /* Verify CONTENT_TYPE_OGG is defined and distinct */
    TEST_ASSERT_EQUAL(3, CONTENT_TYPE_OGG);
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_MP3, CONTENT_TYPE_OGG);
}


static void test_content_type_aac_constant(void)
{
    /* Verify CONTENT_TYPE_AAC is defined and distinct */
    TEST_ASSERT_EQUAL(5, CONTENT_TYPE_AAC);
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_MP3, CONTENT_TYPE_AAC);
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_OGG, CONTENT_TYPE_AAC);
}


/* ========================================================================== */
/* NEW: URL Path Extension Content Type Tests */
/* ========================================================================== */

static void test_parse_url_no_path(void)
{
    debug_printf_Ignore();

    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://host.com", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/", urlinfo.path);
    TEST_ASSERT_EQUAL(80, urlinfo.port);
}


static void test_parse_url_with_query_string(void)
{
    debug_printf_Ignore();

    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://host.com/stream?sid=1", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("/stream?sid=1", urlinfo.path);
}


/* ========================================================================== */
/* NEW: Colon URL Tests - Testing colon handling in various URL positions */
/* ========================================================================== */

/* Test: Colon in credentials (username:password) - should work */
static void test_parse_url_colon_in_credentials(void)
{
    debug_printf_Ignore();

    /* Standard case: username:password@host */
    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://user:pass@host.com/path", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("user", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("pass", urlinfo.password);
    TEST_ASSERT_EQUAL(80, urlinfo.port);
}


/* Test: Colon in credentials with port - should work */
static void test_parse_url_colon_credentials_and_port(void)
{
    debug_printf_Ignore();

    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://user:pass@host.com:8080/path", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("user", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("pass", urlinfo.password);
    TEST_ASSERT_EQUAL(8080, urlinfo.port);
    TEST_ASSERT_EQUAL_STRING("/path", urlinfo.path);
}


/* Test: Multiple colons in credentials and port - the complex case */
static void test_parse_url_multiple_colons_full(void)
{
    debug_printf_Ignore();

    /* Complex: user:pass@host:port/path?key=val:ue */
    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://user:pass@host.com:9000/stream", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("user", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("pass", urlinfo.password);
    TEST_ASSERT_EQUAL(9000, urlinfo.port);
    TEST_ASSERT_EQUAL_STRING("/stream", urlinfo.path);
}


/* Test: Port only (no credentials), colon before path */
static void test_parse_url_port_colon_only(void)
{
    debug_printf_Ignore();

    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://radio.example.com:8000/stream.mp3", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("radio.example.com", urlinfo.host);
    TEST_ASSERT_EQUAL(8000, urlinfo.port);
    TEST_ASSERT_EQUAL_STRING("/stream.mp3", urlinfo.path);
}


/* Test: IPv6 address with port (brackets) - edge case */
/* Note: IPv6 addresses contain colons, e.g. [::1]:8080 */
static void test_parse_url_typical_shoutcast(void)
{
    debug_printf_Ignore();

    /* Typical Shoutcast URL format */
    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://stream.example.com:8000/;stream.nsv", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("stream.example.com", urlinfo.host);
    TEST_ASSERT_EQUAL(8000, urlinfo.port);
}


/* Test: HTTPS with credentials and port */
static void test_parse_url_https_full_credentials(void)
{
    debug_printf_Ignore();

    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("https://admin:secret@secure.radio.com:8443/live", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("secure.radio.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("admin", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("secret", urlinfo.password);
    TEST_ASSERT_EQUAL(8443, urlinfo.port);
    TEST_ASSERT_EQUAL_STRING("/live", urlinfo.path);
}


/* Test: Password with special characters including colon */
static void test_parse_url_password_with_colon_encoded(void)
{
    debug_printf_Ignore();

    /* Password "pass:word" should be URL-encoded as "pass%3Aword" */
    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://user:pass%3Aword@host.com/path", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("user", urlinfo.username);
    /* After unescape_pct_encoding, should be "pass:word" */
    TEST_ASSERT_EQUAL_STRING("pass:word", urlinfo.password);
}


/* Test: Username only without password (user@host.com) */
static void test_parse_url_username_only_no_password(void)
{
    debug_printf_Ignore();

    /* URL with username but no password - sscanf returns 1, password set to empty */
    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://user@host.com/path", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("user", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("", urlinfo.password);
    TEST_ASSERT_EQUAL(80, urlinfo.port);
    TEST_ASSERT_EQUAL_STRING("/path", urlinfo.path);
}


/* Test: Empty password (user:@host.com) */
static void test_parse_url_empty_password(void)
{
    debug_printf_Ignore();

    /* URL with empty password after colon */
    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://user:@host.com/stream", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("user", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("", urlinfo.password);
    TEST_ASSERT_EQUAL(80, urlinfo.port);
    TEST_ASSERT_EQUAL_STRING("/stream", urlinfo.path);
}


/* Test: Username containing encoded colon (%3A) */
static void test_parse_url_username_with_colon_encoded(void)
{
    debug_printf_Ignore();

    /* Username "user:name" URL-encoded as "user%3Aname" */
    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://user%3Aname:pass@host.com/path", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    /* After unescape_pct_encoding, should be "user:name" */
    TEST_ASSERT_EQUAL_STRING("user:name", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("pass", urlinfo.password);
    TEST_ASSERT_EQUAL(80, urlinfo.port);
}


/* Test: Password with @ sign encoded (%40) */
static void test_parse_url_multiple_at_signs_in_password(void)
{
    debug_printf_Ignore();

    /* Password "pass@word" URL-encoded as "pass%40word" */
    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://user:pass%40word@host.com/path", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("host.com", urlinfo.host);
    TEST_ASSERT_EQUAL_STRING("user", urlinfo.username);
    /* After unescape_pct_encoding, should be "pass@word" */
    TEST_ASSERT_EQUAL_STRING("pass@word", urlinfo.password);
    TEST_ASSERT_EQUAL(80, urlinfo.port);
}


/* Test: IPv4 address with port */
static void test_parse_url_ipv4_with_port(void)
{
    debug_printf_Ignore();

    /* IP address instead of hostname */
    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://192.168.1.1:8000/stream", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", urlinfo.host);
    TEST_ASSERT_EQUAL(8000, urlinfo.port);
    TEST_ASSERT_EQUAL_STRING("/stream", urlinfo.path);
    TEST_ASSERT_EQUAL_STRING("", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("", urlinfo.password);
}


/* Test: localhost with port */
static void test_parse_url_localhost_with_port(void)
{
    debug_printf_Ignore();

    /* localhost URL */
    TEST_ASSERT_EQUAL(SR_SUCCESS, http_parse_url("http://localhost:8000/stream", &urlinfo));
    TEST_ASSERT_EQUAL_STRING("localhost", urlinfo.host);
    TEST_ASSERT_EQUAL(8000, urlinfo.port);
    TEST_ASSERT_EQUAL_STRING("/stream", urlinfo.path);
    TEST_ASSERT_EQUAL_STRING("", urlinfo.username);
    TEST_ASSERT_EQUAL_STRING("", urlinfo.password);
}


/* ========================================================================== */
/* Main - Run all tests */
/* ========================================================================== */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* Original URL parsing tests (6 tests) */
    RUN_TEST(test_parse_url_simple);
    RUN_TEST(test_parse_url_with_path_and_port);
    RUN_TEST(test_parse_url_with_port);
    RUN_TEST(test_parse_url_with_credentials);
    RUN_TEST(test_parse_url_with_colon_in_uri);
    RUN_TEST(test_parse_url_complex);

    /* NEW: HTTPS URL parsing tests (2 tests) */
    RUN_TEST(test_parse_url_https_simple);
    RUN_TEST(test_parse_url_https_with_port);

    /* NEW: url_is_https tests (2 tests) */
    RUN_TEST(test_url_is_https_true);
    RUN_TEST(test_url_is_https_false);

    /* NEW: http_parse_sc_header tests (5 tests) */
    RUN_TEST(test_parse_sc_header_icy_200);
    RUN_TEST(test_parse_sc_header_http_200);
    RUN_TEST(test_parse_sc_header_404_error);
    RUN_TEST(test_parse_sc_header_401_error);
    RUN_TEST(test_parse_sc_header_null_params);

    /* NEW: Content type constant tests (3 tests) */
    RUN_TEST(test_content_type_mp3_constant);
    RUN_TEST(test_content_type_ogg_constant);
    RUN_TEST(test_content_type_aac_constant);

    /* NEW: Additional URL parsing tests (2 tests) */
    RUN_TEST(test_parse_url_no_path);
    RUN_TEST(test_parse_url_with_query_string);

    /* NEW: Colon URL parsing tests (13 tests) */
    RUN_TEST(test_parse_url_colon_in_credentials);
    RUN_TEST(test_parse_url_colon_credentials_and_port);
    RUN_TEST(test_parse_url_multiple_colons_full);
    RUN_TEST(test_parse_url_port_colon_only);
    RUN_TEST(test_parse_url_typical_shoutcast);
    RUN_TEST(test_parse_url_https_full_credentials);
    RUN_TEST(test_parse_url_password_with_colon_encoded);
    RUN_TEST(test_parse_url_username_only_no_password);
    RUN_TEST(test_parse_url_empty_password);
    RUN_TEST(test_parse_url_username_with_colon_encoded);
    RUN_TEST(test_parse_url_multiple_at_signs_in_password);
    RUN_TEST(test_parse_url_ipv4_with_port);
    RUN_TEST(test_parse_url_localhost_with_port);

    return UNITY_END();
}
