SET(UNITY_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendor/unity/src)
SET(UNITY_SRC_DIR ${UNITY_INCLUDE_DIR})
SET(UNITY_SRC ${UNITY_SRC_DIR}/unity.c)
INCLUDE_DIRECTORIES(${UNITY_INCLUDE_DIR})

SET(CMOCK_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendor/cmock/src)
SET(CMOCK_SRC_DIR ${CMOCK_INCLUDE_DIR})
SET(CMOCK_SRC ${CMOCK_SRC_DIR}/cmock.c)
INCLUDE_DIRECTORIES(${CMOCK_INCLUDE_DIR})

SET(MOCK_SRC_DIR ${CMAKE_SOURCE_DIR}/tests/mocks)
INCLUDE_DIRECTORIES(${MOCK_SRC_DIR})
ADD_SUBDIRECTORY(http_test)

