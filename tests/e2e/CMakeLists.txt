# End-to-end tests CMakeLists.txt
# Inherits UNITY_SRC, CMOCK_SRC, MOCK_SRC_DIR from parent

# Common include directories are set in parent CMakeLists.txt:
# - UNITY_INCLUDE_DIR (vendor/unity/src)
# - CMOCK_INCLUDE_DIR (vendor/cmock/src)
# - MOCK_SRC_DIR (tests/mocks)
# - Library includes (lib/)

# E2E tests include the test helpers for mock server management
SET(E2E_HELPERS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${E2E_HELPERS_DIR})

# Find required packages for OpenSSL (needed by srtypes.h)
FIND_PACKAGE(OpenSSL REQUIRED)
INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})

# libmad include and library directories (for findsep.c)
SET(LIBMAD_DIR ${PROJECT_ROOT}/libmad-0.15.1b)
SET(LIBMAD_LIB ${LIBMAD_DIR}/.libs/libmad.a)
INCLUDE_DIRECTORIES(${LIBMAD_DIR})

# E2E helper sources (shared by all E2E tests)
SET(E2E_HELPER_SRC
    test_e2e_helpers.c
)

# ============================================================================
# Library sources for E2E tests
# These tests focus on API structures and configuration validation
# Include core library files needed for URL parsing and configuration
# ============================================================================
SET(E2E_LIB_SRC
    ${LIB_DIR}/http.c
    ${LIB_DIR}/errors.c
    ${LIB_DIR}/debug.c
    ${LIB_DIR}/socklib.c
    ${LIB_DIR}/mchar.c
    ${LIB_DIR}/prefs.c
)

# ============================================================================
# test_e2e_http_mp3 - E2E tests for HTTP MP3 streaming and RIP_MANAGER
# ============================================================================
SET(TEST_E2E_HTTP_MP3_SRC
    test_e2e_http_mp3.c
    ${E2E_HELPER_SRC}
    ${E2E_LIB_SRC}
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_e2e_http_mp3 ${TEST_E2E_HTTP_MP3_SRC})
TARGET_LINK_LIBRARIES(test_e2e_http_mp3
    ${GLIB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)
ADD_TEST(NAME e2e_http_mp3 COMMAND test_e2e_http_mp3)

# ============================================================================
# test_e2e_metadata_changes - E2E tests for metadata handling
# ============================================================================
SET(TEST_E2E_METADATA_SRC
    test_e2e_metadata_changes.c
    ${E2E_HELPER_SRC}
    ${E2E_LIB_SRC}
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_e2e_metadata_changes ${TEST_E2E_METADATA_SRC})
TARGET_LINK_LIBRARIES(test_e2e_metadata_changes
    ${GLIB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)
ADD_TEST(NAME e2e_metadata_changes COMMAND test_e2e_metadata_changes)

# ============================================================================
# test_e2e_reconnect - E2E tests for reconnection configuration
# ============================================================================
SET(TEST_E2E_RECONNECT_SRC
    test_e2e_reconnect.c
    ${E2E_HELPER_SRC}
    ${E2E_LIB_SRC}
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_e2e_reconnect ${TEST_E2E_RECONNECT_SRC})
TARGET_LINK_LIBRARIES(test_e2e_reconnect
    ${GLIB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)
ADD_TEST(NAME e2e_reconnect COMMAND test_e2e_reconnect)

# ============================================================================
# Copy mock_server.py to build directory for test execution
# ============================================================================
CONFIGURE_FILE(
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_server.py
    ${CMAKE_CURRENT_BINARY_DIR}/mock_server.py
    COPYONLY
)
