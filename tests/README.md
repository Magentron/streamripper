# Streamripper Test Suite

Comprehensive test suite for Streamripper using the Unity testing framework and CMock for mocking.

## Overview

The test suite is organized into three levels:

| Level | Directory | Description | Test Count |
|-------|-----------|-------------|------------|
| Unit | `unit/` | Isolated module tests with mocked dependencies | 18 |
| HTTP | `http_test/` | Comprehensive HTTP protocol tests | 165 |
| Integration | `integration/` | Component interaction tests across modules | 4 |
| E2E | `e2e/` | End-to-end tests with mock HTTP server | 3 |

**Total Tests: 190** (26 CTest suites + 165 http_test cases)

## Directory Structure

```
tests/
├── CMakeLists.txt          # Main CMake configuration
├── run_coverage.sh         # Coverage report generation script
├── recreate_mocks.py       # Mock generation script
├── generate_mocks.py       # Alternative mock generator
├── cmock_cfg.yml           # CMock configuration
├── test_helpers.h          # Common test utilities and macros
├── mocks/                  # Generated mock files
├── fixtures/               # Test fixtures (sample data)
├── http_test/              # Comprehensive HTTP protocol tests (165 tests)
│   ├── CMakeLists.txt
│   └── http_test.c         # URL parsing, headers, requests, playlists
├── unit/                   # Unit tests (17 test suites)
│   ├── CMakeLists.txt
│   ├── test_cbuf2.c        # Circular buffer tests
│   ├── test_charset.c      # Character set conversion tests
│   ├── test_debug.c        # Debug output tests
│   ├── test_errors.c       # Error handling tests
│   ├── test_external.c     # External command execution tests
│   ├── test_filelib.c      # File operations tests
│   ├── test_findsep.c      # Silence/separator detection tests
│   ├── test_https.c        # HTTPS connection tests
│   ├── test_mchar.c        # Multi-byte character tests
│   ├── test_parse.c        # Metadata parsing tests
│   ├── test_prefs.c        # Preferences handling tests
│   ├── test_relaylib.c     # Relay server tests
│   ├── test_ripaac.c       # AAC format parsing tests
│   ├── test_ripogg.c       # OGG/Vorbis format tests
│   ├── test_security_fixes.c # Security regression tests
│   ├── test_socklib.c      # Socket library tests
│   ├── test_threadlib.c    # Thread library tests
│   └── test_utf8.c         # UTF-8 encoding/decoding tests
├── integration/            # Integration tests
│   ├── CMakeLists.txt
│   ├── test_streaming_pipeline.c
│   ├── test_metadata_pipeline.c
│   ├── test_mp3_silence_detection.c
│   └── test_charset_conversion_chain.c
└── e2e/                    # End-to-end tests
    ├── CMakeLists.txt
    ├── mock_server.py      # Python mock HTTP server
    ├── test_e2e_helpers.c  # E2E test utilities
    ├── test_e2e_helpers.h
    ├── test_e2e_http_mp3.c
    ├── test_e2e_metadata_changes.c
    └── test_e2e_reconnect.c
```

## Prerequisites

- **CMake** 3.10 or later
- **GCC** or **Clang** with C99 support
- **GLib 2.0** development libraries
- **pkg-config**
- **lcov** (optional, for coverage reports)
- **Python 3** (optional, for E2E mock server)

### Installing Dependencies

**Debian/Ubuntu:**
```bash
sudo apt install cmake build-essential libglib2.0-dev pkg-config lcov python3
```

**Fedora/RHEL:**
```bash
sudo dnf install cmake gcc glib2-devel pkgconfig lcov python3
```

**macOS (Homebrew):**
```bash
brew install cmake glib pkg-config lcov python3
```

## Building the Tests

### Standard Build

```bash
cd tests
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Build with Coverage

```bash
cd tests
mkdir -p build && cd build
cmake -DENABLE_COVERAGE=ON ..
make -j$(nproc)
```

## Running Tests

### Run All Tests

```bash
cd tests/build
ctest --output-on-failure
```

### Run Tests by Category

```bash
# Unit tests only
ctest -R "test_" --output-on-failure

# Integration tests only
ctest -R "integration_" --output-on-failure

# E2E tests only
ctest -R "e2e_" --output-on-failure
```

### Run a Specific Test

```bash
# By name
ctest -R test_charset --output-on-failure

# Direct execution (shows detailed output)
./test_charset
```

### Verbose Output

```bash
ctest -V --output-on-failure
```

## Code Coverage

### Current Coverage Results

**Overall: 86.3% line coverage, 96.7% function coverage**

| File | Line Coverage | Function Coverage |
|------|---------------|-------------------|
| errors.c | 100.0% | 100.0% |
| threadlib.c | 100.0% | 100.0% |
| list.h | 100.0% | 100.0% |
| utf8.c | 96.4% | 100.0% |
| charset.c | 95.1% | 100.0% |
| cbuf2.c | 92.9% | 100.0% |
| debug.c | 91.7% | 83.3% |
| mchar.c | 91.7% | 100.0% |
| parse.c | 90.7% | 100.0% |
| external.c | 89.1% | 100.0% |
| socklib.c | 88.8% | 100.0% |
| prefs.c | 87.7% | 100.0% |
| argv.c | 87.5% | 100.0% |
| http.c | 83.5% | 85.7% |
| filelib.c | 80.5% | 97.1% |
| relaylib.c | 61.6%* | 78.6% |

*relaylib.c contains thread functions (`thread_accept`, `thread_send`) with infinite loops that require integration tests to cover.

### Using the Coverage Script (Recommended)

```bash
cd tests
./run_coverage.sh
```

This script will:
1. Configure CMake with coverage flags
2. Build all tests
3. Run all tests
4. Generate an HTML coverage report

The report is generated at: `tests/build/coverage_html/index.html`

### Manual Coverage Generation

```bash
cd tests
mkdir -p build && cd build

# Build with coverage
cmake -DENABLE_COVERAGE=ON ..
make -j$(nproc)

# Run tests
ctest --output-on-failure

# Generate coverage data
lcov --capture --directory . --output-file coverage.info

# Remove external code from report
lcov --remove coverage.info '/usr/*' '*/vendor/*' '*/tests/*' --output-file coverage.info

# Generate HTML report
genhtml coverage.info --output-directory coverage_html

# View report
xdg-open coverage_html/index.html   # Linux
open coverage_html/index.html        # macOS
```

### Using CMake Coverage Target

```bash
cd tests/build
cmake -DENABLE_COVERAGE=ON ..
make
ctest
make coverage
```

## Test Frameworks

### Unity

[Unity](https://github.com/ThrowTheSwitch/Unity) is a lightweight C testing framework. Tests use the following assertions:

```c
TEST_ASSERT_EQUAL(expected, actual);
TEST_ASSERT_EQUAL_STRING("expected", actual);
TEST_ASSERT_TRUE(condition);
TEST_ASSERT_FALSE(condition);
TEST_ASSERT_NULL(pointer);
TEST_ASSERT_NOT_NULL(pointer);
TEST_ASSERT_EQUAL_MEMORY(expected, actual, size);
```

### CMock

[CMock](https://github.com/ThrowTheSwitch/CMock) generates mock functions for testing. Example usage:

```c
#include "Mocksocklib.h"

void test_example(void)
{
    // Expect a function call and set return value
    socklib_init_ExpectAndReturn(SR_SUCCESS);

    // Ignore a function (don't verify it's called)
    socklib_close_Ignore();

    // Test your code
    error_code result = my_function();
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
}
```

## Regenerating Mocks

If you modify header files, regenerate the mocks:

```bash
cd tests
python3 recreate_mocks.py
```

## Test Organization Conventions

### Test File Naming

- Unit tests: `test_<module>.c` (e.g., `test_charset.c`)
- Integration tests: `test_<feature>_<description>.c`
- E2E tests: `test_e2e_<scenario>.c`

### Test Function Naming

```c
// Pattern: test_<function>_<scenario>
static void test_charset_convert_valid_utf8(void);
static void test_charset_convert_invalid_input(void);
static void test_charset_convert_null_pointer(void);
```

### Test Structure (AAA Pattern)

```c
static void test_function_scenario(void)
{
    /* Arrange - Set up test data and expectations */
    char input[] = "test data";
    mock_function_ExpectAndReturn(value);

    /* Act - Execute the function under test */
    int result = function_under_test(input);

    /* Assert - Verify the results */
    TEST_ASSERT_EQUAL(EXPECTED_VALUE, result);
}
```

## Test Suites

### Unit Tests (18 test suites)

| Test | Module | Description |
|------|--------|-------------|
| test_cbuf2 | cbuf2.c | Circular buffer implementation |
| test_charset | charset.c | Character set detection and conversion |
| test_debug | debug.c | Debug output and logging |
| test_errors | errors.c | Error code handling and messages |
| test_external | external.c, argv.c | External command execution |
| test_filelib | filelib.c | File operations and path handling |
| test_findsep | findsep.c | Silence and track separator detection |
| test_https | https.c | HTTPS/SSL connection handling |
| test_mchar | mchar.c | Multi-byte character operations |
| test_parse | parse.c | Metadata parsing with rules |
| test_prefs | prefs.c | Preferences parsing and storage |
| test_relaylib | relaylib.c | Relay server socket operations |
| test_ripaac | ripaac.c | AAC audio format parsing |
| test_ripogg | ripogg.c | OGG/Vorbis format parsing |
| test_security_fixes | multiple | Security vulnerability regression tests |
| test_socklib | socklib.c | Low-level socket operations |
| test_threadlib | threadlib.c | Thread and semaphore operations |
| test_utf8 | utf8.c | UTF-8 encoding and decoding |

### HTTP Tests (165 tests)

| Category | Description |
|----------|-------------|
| URL Parsing | Protocol, host, port, path, credentials extraction |
| Header Parsing | ICY/HTTP headers, content types, metadata intervals |
| Request Construction | Stream requests, page requests, proxy support |
| Response Construction | ICY responses with metadata support |
| Playlist Parsing | PLS and M3U playlist formats |
| Error Handling | HTTP status codes, connection errors |

### Integration Tests (4 tests)

| Test | Description |
|------|-------------|
| integration_streaming_pipeline | HTTP connect -> buffer -> file write |
| integration_metadata_pipeline | Parse ICY metadata -> apply rules -> filename |
| integration_mp3_silence_detection | Buffer -> silence detection chain |
| integration_charset_conversion_chain | Charset -> UTF-8 -> filename generation |

### E2E Tests (3 tests)

| Test | Description |
|------|-------------|
| e2e_http_mp3 | Complete MP3 streaming capture |
| e2e_metadata_changes | Track splitting on metadata changes |
| e2e_reconnect | Connection recovery after disconnect |

## Troubleshooting

### CMake Configuration Fails

```
Could not find PkgConfig
```
Install pkg-config: `sudo apt install pkg-config`

```
Could not find glib-2.0
```
Install GLib development files: `sudo apt install libglib2.0-dev`

### Tests Don't Build

Ensure you're building from the correct directory:
```bash
cd /path/to/streamripper/tests
mkdir -p build && cd build
cmake ..
make
```

### Coverage Report Empty

1. Ensure you built with `-DENABLE_COVERAGE=ON`
2. Run tests before generating coverage
3. Check that lcov is installed: `which lcov`

### Mock Files Missing

Regenerate mocks:
```bash
cd tests
python3 recreate_mocks.py
```

### E2E Tests Fail

E2E tests require Python 3 for the mock server:
```bash
python3 --version  # Should be 3.6+
```

## Adding New Tests

1. Create test file in appropriate directory (`unit/`, `integration/`, or `e2e/`)
2. Add test to the corresponding `CMakeLists.txt`
3. Follow existing test patterns and naming conventions
4. Regenerate mocks if needed

Example CMakeLists.txt entry:
```cmake
SET(TEST_NEWMODULE_SRC
    test_newmodule.c
    ${LIB_DIR}/newmodule.c
    ${UNITY_SRC}
    ${CMOCK_SRC}
    ${MOCK_SRC_DIR}/Mockdependency.c
)
ADD_EXECUTABLE(test_newmodule ${TEST_NEWMODULE_SRC})
TARGET_LINK_LIBRARIES(test_newmodule ${GLIB_LIBRARIES})
ADD_TEST(NAME test_newmodule COMMAND test_newmodule)
```

## License

Tests are part of the Streamripper project and follow the same license terms.
