# Unit tests CMakeLists.txt
# Inherits UNITY_SRC, CMOCK_SRC, MOCK_SRC_DIR from parent

# Common include directories are set in parent CMakeLists.txt:
# - UNITY_INCLUDE_DIR (vendor/unity/src)
# - CMOCK_INCLUDE_DIR (vendor/cmock/src)
# - MOCK_SRC_DIR (tests/mocks)
# - Library includes (lib/)

# Find required packages for OpenSSL (needed by srtypes.h)
FIND_PACKAGE(OpenSSL REQUIRED)
INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})

# ============================================================================
# test_errors - Tests for lib/errors.c
# ============================================================================
SET(TEST_ERRORS_SRC
    test_errors.c
    ${LIB_DIR}/errors.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_errors ${TEST_ERRORS_SRC})
TARGET_LINK_LIBRARIES(test_errors ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_errors COMMAND test_errors)

# ============================================================================
# test_mchar - Tests for lib/mchar.c
# ============================================================================
SET(TEST_MCHAR_SRC
    test_mchar.c
    ${LIB_DIR}/mchar.c
    ${MOCK_SRC_DIR}/Mockdebug.c
    ${UNITY_SRC}
    ${CMOCK_SRC}
)
ADD_EXECUTABLE(test_mchar ${TEST_MCHAR_SRC})
TARGET_LINK_LIBRARIES(test_mchar ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_mchar COMMAND test_mchar)

# ============================================================================
# test_debug - Tests for lib/debug.c
# ============================================================================
# Note: test_debug.c provides stub implementations for sr_strncpy and threadlib
# semaphore functions to avoid circular dependencies with mchar.c and threadlib.c
SET(TEST_DEBUG_SRC
    test_debug.c
    ${LIB_DIR}/debug.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_debug ${TEST_DEBUG_SRC})
TARGET_LINK_LIBRARIES(test_debug ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES} pthread)
ADD_TEST(NAME test_debug COMMAND test_debug)

# ============================================================================
# test_charset - Tests for charset conversion functions
# ============================================================================
SET(TEST_CHARSET_SRC
    test_charset.c
    ${LIB_DIR}/charset.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_charset ${TEST_CHARSET_SRC})
TARGET_LINK_LIBRARIES(test_charset ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_charset COMMAND test_charset)

# ============================================================================
# test_utf8 - Tests for UTF-8 encoding/decoding functions
# ============================================================================
# Note: This test uses error injection to test charset_convert error paths
# charset_testable.c wraps charset.c and allows error injection for testing
SET(TEST_UTF8_SRC
    test_utf8.c
    ${LIB_DIR}/utf8.c
    charset_testable.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_utf8 ${TEST_UTF8_SRC})
TARGET_INCLUDE_DIRECTORIES(test_utf8 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(test_utf8 ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_utf8 COMMAND test_utf8)

# ============================================================================
# test_filelib - Tests for lib/filelib.c I/O operations
# ============================================================================
# Note: test_filelib.c provides stub implementations for debug and mchar
# functions to avoid complex dependencies while testing actual filelib code.
SET(TEST_FILELIB_SRC
    test_filelib.c
    ${LIB_DIR}/filelib.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_filelib ${TEST_FILELIB_SRC})
TARGET_LINK_LIBRARIES(test_filelib ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_filelib COMMAND test_filelib)

# ============================================================================
# test_prefs - Tests for lib/prefs.c preference management
# ============================================================================
# Note: test_prefs.c provides stub implementations for debug_printf and mchar
# functions (set_codesets_default) to avoid complex dependencies. Also provides
# stub for url_is_https from http.c and helper functions for overwrite option
# conversion. Tests actual prefs.c code for load/save and get/set functions.
SET(TEST_PREFS_SRC
    test_prefs.c
    ${LIB_DIR}/prefs.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_prefs ${TEST_PREFS_SRC})
TARGET_LINK_LIBRARIES(test_prefs ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_prefs COMMAND test_prefs)

# ============================================================================
# test_parse - Tests for lib/parse.c metadata parsing functions
# ============================================================================
# Note: test_parse.c provides stub implementations for debug and mchar
# functions to avoid complex dependencies while testing actual parse code.
SET(TEST_PARSE_SRC
    test_parse.c
    ${LIB_DIR}/parse.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_parse ${TEST_PARSE_SRC})
TARGET_LINK_LIBRARIES(test_parse ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_parse COMMAND test_parse)

# ============================================================================
# test_https - Tests for lib/https.c structures
# ============================================================================
SET(TEST_HTTPS_SRC
    test_https.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_https ${TEST_HTTPS_SRC})
TARGET_LINK_LIBRARIES(test_https ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_https COMMAND test_https)

# ============================================================================
# test_socklib - Tests for lib/socklib.c network operations
# ============================================================================
# Note: This test uses real socket operations with mock echo servers running
# on localhost to test socklib_open, socklib_close, socklib_sendall,
# socklib_recvall, and socklib_read_header functions.
SET(TEST_SOCKLIB_SRC
    test_socklib.c
    ${LIB_DIR}/socklib.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_socklib ${TEST_SOCKLIB_SRC})
TARGET_LINK_LIBRARIES(test_socklib ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES} pthread)
ADD_TEST(NAME test_socklib COMMAND test_socklib)

# ============================================================================
# Phase 4: Audio Processing Layer Tests
# ============================================================================

# ============================================================================
# test_cbuf2 - Tests for lib/cbuf2.c (circular buffer)
# ============================================================================
# Note: test_cbuf2.c provides stub implementations for threadlib, debug, and
# relaylib functions to avoid complex dependencies
SET(TEST_CBUF2_SRC
    test_cbuf2.c
    ${LIB_DIR}/cbuf2.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_cbuf2 ${TEST_CBUF2_SRC})
TARGET_LINK_LIBRARIES(test_cbuf2 ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_cbuf2 COMMAND test_cbuf2)

# ============================================================================
# test_findsep - Tests for silence detection data structures
# ============================================================================
# Note: The actual findsep functions require libmad for MP3 decoding.
# These tests focus on data structure validation, SPLITPOINT_OPTIONS,
# and MP3 frame header parsing logic (without actually calling findsep functions)
SET(TEST_FINDSEP_SRC
    test_findsep.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_findsep ${TEST_FINDSEP_SRC})
TARGET_LINK_LIBRARIES(test_findsep ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_findsep COMMAND test_findsep)

# ============================================================================
# test_ripaac - Tests for lib/ripaac.c (AAC parsing structures)
# ============================================================================
# Note: ripaac actual code is conditionally compiled with HAVE_FAAD
# These tests verify data structures and ADTS header parsing
SET(TEST_RIPAAC_SRC
    test_ripaac.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_ripaac ${TEST_RIPAAC_SRC})
TARGET_LINK_LIBRARIES(test_ripaac ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_ripaac COMMAND test_ripaac)

# ============================================================================
# test_ripogg - Tests for lib/ripogg.c (OGG parsing structures)
# ============================================================================
# Note: ripogg actual code is conditionally compiled with HAVE_OGG_VORBIS
# These tests verify data structures and OGG page parsing
SET(TEST_RIPOGG_SRC
    test_ripogg.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_ripogg ${TEST_RIPOGG_SRC})
TARGET_LINK_LIBRARIES(test_ripogg ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_ripogg COMMAND test_ripogg)

# ============================================================================
# test_threadlib - Tests for lib/threadlib.c (threading and semaphores)
# ============================================================================
# Note: This test uses real threading primitives (pthreads) rather than mocks
# since threadlib provides thin wrappers that need real behavior testing.
SET(TEST_THREADLIB_SRC
    test_threadlib.c
    ${LIB_DIR}/threadlib.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_threadlib ${TEST_THREADLIB_SRC})
TARGET_LINK_LIBRARIES(test_threadlib ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES} pthread)
ADD_TEST(NAME test_threadlib COMMAND test_threadlib)

# ============================================================================
# test_external - Tests for lib/external.c (external process spawning)
# ============================================================================
# Note: test_external.c provides stub implementations for debug_printf and
# mchar functions (gstring_from_string, string_from_gstring) to avoid
# complex dependencies. Tests actual fork/pipe/exec behavior.
SET(TEST_EXTERNAL_SRC
    test_external.c
    ${LIB_DIR}/external.c
    ${LIB_DIR}/argv.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_external ${TEST_EXTERNAL_SRC})
TARGET_LINK_LIBRARIES(test_external ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_external COMMAND test_external)

# ============================================================================
# test_relaylib - Tests for lib/relaylib.c (relay server)
# ============================================================================
# Note: test_relaylib.c provides stub implementations for threadlib, debug,
# cbuf2, and rip_manager functions to avoid complex dependencies and socket
# operations. Tests the relay server data structures, list management, and
# parameter validation.
#
# RELAYLIB_TESTING exposes internal static functions for direct testing.
SET(TEST_RELAYLIB_SRC
    test_relaylib.c
    ${LIB_DIR}/relaylib.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_relaylib ${TEST_RELAYLIB_SRC})
TARGET_COMPILE_DEFINITIONS(test_relaylib PRIVATE RELAYLIB_TESTING=1)
TARGET_LINK_LIBRARIES(test_relaylib ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES} pthread)
ADD_TEST(NAME test_relaylib COMMAND test_relaylib)
