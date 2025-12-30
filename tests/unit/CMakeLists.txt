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
SET(TEST_UTF8_SRC
    test_utf8.c
    ${LIB_DIR}/utf8.c
    ${LIB_DIR}/charset.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_utf8 ${TEST_UTF8_SRC})
TARGET_LINK_LIBRARIES(test_utf8 ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_utf8 COMMAND test_utf8)

# ============================================================================
# test_filelib - Tests for lib/filelib.c structures and macros
# ============================================================================
SET(TEST_FILELIB_SRC
    test_filelib.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_filelib ${TEST_FILELIB_SRC})
TARGET_LINK_LIBRARIES(test_filelib ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_filelib COMMAND test_filelib)

# ============================================================================
# test_prefs - Tests for lib/prefs.c structures
# ============================================================================
SET(TEST_PREFS_SRC
    test_prefs.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_prefs ${TEST_PREFS_SRC})
TARGET_LINK_LIBRARIES(test_prefs ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
ADD_TEST(NAME test_prefs COMMAND test_prefs)

# ============================================================================
# test_parse - Tests for lib/parse.c structures
# ============================================================================
SET(TEST_PARSE_SRC
    test_parse.c
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
# test_socklib - Tests for lib/socklib.c structures
# ============================================================================
SET(TEST_SOCKLIB_SRC
    test_socklib.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_socklib ${TEST_SOCKLIB_SRC})
TARGET_LINK_LIBRARIES(test_socklib ${GLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
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
