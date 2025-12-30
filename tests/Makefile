# Streamripper Tests Makefile
# Convenient targets for building and running tests

.PHONY: all build test test-unit test-integration test-e2e coverage clean rebuild help

BUILD_DIR = build
CMAKE_FLAGS = -DENABLE_COVERAGE=OFF

# Default target
all: build test

# Build all tests
build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) .. && make -j$$(nproc)

# Build with coverage enabled
build-coverage:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DENABLE_COVERAGE=ON .. && make -j$$(nproc)

# Run all tests
test: build
	@cd $(BUILD_DIR) && ctest --output-on-failure

# Run only unit tests
test-unit: build
	@cd $(BUILD_DIR) && ctest --output-on-failure -R "^test_"

# Run only integration tests
test-integration: build
	@cd $(BUILD_DIR) && ctest --output-on-failure -R "^integration_"

# Run only E2E tests
test-e2e: build
	@cd $(BUILD_DIR) && ctest --output-on-failure -R "^e2e_"

# Run HTTP tests only
test-http: build
	@cd $(BUILD_DIR) && ctest --output-on-failure -R "http"

# Run a specific test (usage: make run TEST=test_errors)
run: build
	@if [ -z "$(TEST)" ]; then \
		echo "Usage: make run TEST=test_name"; \
		echo "Available tests:"; \
		cd $(BUILD_DIR) && ctest -N | grep "Test #" | sed 's/.*: //'; \
	else \
		cd $(BUILD_DIR) && ctest --output-on-failure -R "$(TEST)"; \
	fi

# Run tests with verbose output
test-verbose: build
	@cd $(BUILD_DIR) && ctest -V

# Generate coverage report
coverage: build-coverage
	@cd $(BUILD_DIR) && ctest --output-on-failure
	@if command -v lcov > /dev/null; then \
		cd $(BUILD_DIR) && \
		lcov --capture --directory . --output-file coverage.info && \
		lcov --remove coverage.info '/usr/*' '*/vendor/*' '*/tests/*' --output-file coverage.info && \
		genhtml coverage.info --output-directory coverage_html && \
		echo "Coverage report: $(BUILD_DIR)/coverage_html/index.html"; \
	else \
		echo "lcov not installed. Install with: sudo apt install lcov"; \
	fi

# List all available tests
list:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) .. > /dev/null 2>&1
	@cd $(BUILD_DIR) && ctest -N | grep "Test #"

# Clean build directory
clean:
	@rm -rf $(BUILD_DIR)

# Rebuild from scratch
rebuild: clean build

# Regenerate mocks (requires Ruby)
mocks:
	@python3 recreate_mocks.py

# Help
help:
	@echo "Streamripper Test Suite"
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Build targets:"
	@echo "  build          - Build all tests"
	@echo "  build-coverage - Build with coverage instrumentation"
	@echo "  rebuild        - Clean and rebuild"
	@echo "  clean          - Remove build directory"
	@echo "  mocks          - Regenerate CMock mocks (requires Ruby)"
	@echo ""
	@echo "Test targets:"
	@echo "  test           - Build and run all tests"
	@echo "  test-unit      - Run unit tests only"
	@echo "  test-integration - Run integration tests only"
	@echo "  test-e2e       - Run E2E tests only"
	@echo "  test-http      - Run HTTP-related tests"
	@echo "  test-verbose   - Run tests with verbose output"
	@echo "  run TEST=name  - Run a specific test"
	@echo "  list           - List all available tests"
	@echo ""
	@echo "Coverage:"
	@echo "  coverage       - Build with coverage and generate HTML report"
	@echo ""
	@echo "Examples:"
	@echo "  make test                    # Run all tests"
	@echo "  make run TEST=test_errors    # Run specific test"
	@echo "  make coverage                # Generate coverage report"
