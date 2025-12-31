CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(streamripper_tests)

# Project root (parent of tests directory)
SET(PROJECT_ROOT ${CMAKE_SOURCE_DIR}/..)
GET_FILENAME_COMPONENT(PROJECT_ROOT ${PROJECT_ROOT} ABSOLUTE)

# Platform defines (same as configure.ac)
ADD_DEFINITIONS(-D__UNIX__)

# Coverage option
OPTION(ENABLE_COVERAGE "Enable code coverage" OFF)
IF(ENABLE_COVERAGE)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage -O0 -g")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov")
ENDIF()

# Unity framework
SET(UNITY_INCLUDE_DIR ${PROJECT_ROOT}/vendor/unity/src)
SET(UNITY_SRC ${UNITY_INCLUDE_DIR}/unity.c)
INCLUDE_DIRECTORIES(${UNITY_INCLUDE_DIR})

# CMock framework
SET(CMOCK_INCLUDE_DIR ${PROJECT_ROOT}/vendor/cmock/src)
SET(CMOCK_SRC ${CMOCK_INCLUDE_DIR}/cmock.c)
INCLUDE_DIRECTORIES(${CMOCK_INCLUDE_DIR})

# Mock sources
SET(MOCK_SRC_DIR ${CMAKE_SOURCE_DIR}/mocks)
INCLUDE_DIRECTORIES(${MOCK_SRC_DIR})

# Library includes
SET(LIB_DIR ${PROJECT_ROOT}/lib)
INCLUDE_DIRECTORIES(${LIB_DIR})

# Test helpers include
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

# GLib (required by streamripper)
FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GLIB REQUIRED glib-2.0)
INCLUDE_DIRECTORIES(${GLIB_INCLUDE_DIRS})
LINK_DIRECTORIES(${GLIB_LIBRARY_DIRS})

# Enable testing
ENABLE_TESTING()

# Subdirectories
ADD_SUBDIRECTORY(http_test)
ADD_SUBDIRECTORY(unit)
ADD_SUBDIRECTORY(integration)
ADD_SUBDIRECTORY(e2e)

# Coverage target
IF(ENABLE_COVERAGE)
    ADD_CUSTOM_TARGET(coverage
        COMMAND lcov --capture --directory . --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' '*/vendor/*' --output-file coverage.info
        COMMAND genhtml coverage.info --output-directory coverage_html
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage report..."
    )
ENDIF()
