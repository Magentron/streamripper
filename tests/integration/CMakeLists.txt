# Integration tests CMakeLists.txt
# Inherits UNITY_SRC, CMOCK_SRC, MOCK_SRC_DIR from parent
#
# Integration tests exercise real module interactions with minimal mocking.
# They test data flow between multiple modules in the streamripper pipeline.

# Common include directories are set in parent CMakeLists.txt:
# - UNITY_INCLUDE_DIR (vendor/unity/src)
# - CMOCK_INCLUDE_DIR (vendor/cmock/src)
# - MOCK_SRC_DIR (tests/mocks)
# - Library includes (lib/)

# Find required packages
FIND_PACKAGE(OpenSSL REQUIRED)
INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})

# ============================================================================
# test_streaming_pipeline - HTTP -> Parse -> Buffer integration
# ============================================================================
# Tests the streaming pipeline: URL parsing, HTTP header parsing, and
# circular buffer operations working together.
SET(TEST_STREAMING_PIPELINE_SRC
    test_streaming_pipeline.c
    ${LIB_DIR}/http.c
    ${LIB_DIR}/cbuf2.c
    ${LIB_DIR}/mchar.c
    ${LIB_DIR}/errors.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_streaming_pipeline ${TEST_STREAMING_PIPELINE_SRC})
TARGET_LINK_LIBRARIES(test_streaming_pipeline
    ${GLIB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)
ADD_TEST(NAME integration_streaming_pipeline COMMAND test_streaming_pipeline)

# ============================================================================
# test_metadata_pipeline - Metadata parsing -> Track info integration
# ============================================================================
# Tests the metadata pipeline: parsing ICY metadata, applying rules,
# and generating track information with proper encoding.
SET(TEST_METADATA_PIPELINE_SRC
    test_metadata_pipeline.c
    ${LIB_DIR}/parse.c
    ${LIB_DIR}/mchar.c
    ${LIB_DIR}/errors.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_metadata_pipeline ${TEST_METADATA_PIPELINE_SRC})
TARGET_LINK_LIBRARIES(test_metadata_pipeline
    ${GLIB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)
ADD_TEST(NAME integration_metadata_pipeline COMMAND test_metadata_pipeline)

# ============================================================================
# test_mp3_silence_detection - Buffer -> Silence detection integration
# ============================================================================
# Tests the MP3 silence detection pipeline: circular buffer operations
# feeding into silence detection configuration and split point options.
# Note: Does not use libmad directly - tests data structure integration.
SET(TEST_MP3_SILENCE_DETECTION_SRC
    test_mp3_silence_detection.c
    ${LIB_DIR}/cbuf2.c
    ${LIB_DIR}/errors.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_mp3_silence_detection ${TEST_MP3_SILENCE_DETECTION_SRC})
TARGET_LINK_LIBRARIES(test_mp3_silence_detection
    ${GLIB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)
ADD_TEST(NAME integration_mp3_silence_detection COMMAND test_mp3_silence_detection)

# ============================================================================
# test_charset_conversion_chain - Charset -> UTF-8 -> mchar integration
# ============================================================================
# Tests the character encoding conversion chain: charset detection,
# UTF-8 encoding/decoding, and mchar string operations for filename generation.
SET(TEST_CHARSET_CONVERSION_CHAIN_SRC
    test_charset_conversion_chain.c
    ${LIB_DIR}/charset.c
    ${LIB_DIR}/utf8.c
    ${LIB_DIR}/mchar.c
    ${LIB_DIR}/errors.c
    ${UNITY_SRC}
)
ADD_EXECUTABLE(test_charset_conversion_chain ${TEST_CHARSET_CONVERSION_CHAIN_SRC})
TARGET_LINK_LIBRARIES(test_charset_conversion_chain
    ${GLIB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)
ADD_TEST(NAME integration_charset_conversion_chain COMMAND test_charset_conversion_chain)

# ============================================================================
# Fixture directory for test data
# ============================================================================
# Fixtures are located in: tests/fixtures/http_responses/
# - icy_response.txt - Sample ICY protocol response
# - http_response.txt - Sample HTTP/1.0 response
# - redirect_response.txt - Sample HTTP redirect response
