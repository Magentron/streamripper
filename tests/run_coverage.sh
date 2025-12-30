#!/bin/bash
#
# run_coverage.sh - Build and run tests with code coverage
#
# This script builds the Streamripper test suite with coverage instrumentation,
# runs all tests, and generates an HTML coverage report using lcov/genhtml.
#
# Usage: ./run_coverage.sh
#
# Requirements:
#   - CMake 3.10+
#   - GCC/Clang with gcov support
#   - lcov (for coverage report generation)
#   - genhtml (part of lcov package)
#
# Output:
#   - Coverage report: build/coverage_html/index.html
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "=============================================="
echo "Streamripper Test Coverage Report Generator"
echo "=============================================="
echo ""

# Step 1: Create and enter build directory
echo "=== Step 1: Configuring build with coverage flags ==="
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake -DENABLE_COVERAGE=ON ..
echo ""

# Step 2: Build the tests
echo "=== Step 2: Building tests ==="
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
echo ""

# Step 3: Run all tests
echo "=== Step 3: Running tests ==="
ctest --output-on-failure
CTEST_EXIT_CODE=$?
echo ""

if [ $CTEST_EXIT_CODE -ne 0 ]; then
    echo "WARNING: Some tests failed. Coverage report will still be generated."
    echo ""
fi

# Step 4: Generate coverage report
echo "=== Step 4: Generating coverage report ==="

if command -v lcov &> /dev/null; then
    echo "Found lcov, generating coverage report..."

    # Capture coverage data
    lcov --capture --directory . --output-file coverage.info --quiet

    # Remove coverage for system headers, vendor code, and test code
    lcov --remove coverage.info \
        '/usr/*' \
        '*/vendor/*' \
        '*/tests/*' \
        --output-file coverage.info \
        --quiet

    # Generate HTML report
    if command -v genhtml &> /dev/null; then
        genhtml coverage.info \
            --output-directory coverage_html \
            --title "Streamripper Test Coverage" \
            --legend \
            --show-details \
            --quiet

        echo ""
        echo "=============================================="
        echo "Coverage Report Generated Successfully!"
        echo "=============================================="
        echo ""
        echo "Report location: ${BUILD_DIR}/coverage_html/index.html"
        echo ""

        # Print summary statistics
        echo "=== Coverage Summary ==="
        lcov --summary coverage.info 2>&1 | grep -E "(lines|functions|branches)"
        echo ""

        # Try to open the report (optional)
        if [ -n "$DISPLAY" ] && command -v xdg-open &> /dev/null; then
            read -p "Open coverage report in browser? [y/N] " -n 1 -r
            echo ""
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                xdg-open "${BUILD_DIR}/coverage_html/index.html"
            fi
        elif command -v open &> /dev/null; then
            # macOS
            read -p "Open coverage report in browser? [y/N] " -n 1 -r
            echo ""
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                open "${BUILD_DIR}/coverage_html/index.html"
            fi
        fi
    else
        echo "ERROR: genhtml not found. Cannot generate HTML report."
        echo "Install lcov package: sudo apt install lcov (Debian/Ubuntu)"
        echo "                      brew install lcov (macOS)"
        exit 1
    fi
else
    echo "WARNING: lcov not found. Coverage report cannot be generated."
    echo ""
    echo "To install lcov:"
    echo "  Debian/Ubuntu: sudo apt install lcov"
    echo "  Fedora/RHEL:   sudo dnf install lcov"
    echo "  macOS:         brew install lcov"
    echo ""
    echo "You can still view raw gcov data using:"
    echo "  gcov -o <object_dir> <source_file>"
fi

# Return appropriate exit code
if [ $CTEST_EXIT_CODE -ne 0 ]; then
    exit 1
fi

exit 0
