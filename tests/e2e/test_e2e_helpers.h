/* test_e2e_helpers.h
 *
 * Helper functions for E2E tests in Streamripper.
 * Provides utilities for:
 * - Starting/stopping the Python mock server
 * - Verifying output files
 * - Managing test fixtures
 */

#ifndef TEST_E2E_HELPERS_H
#define TEST_E2E_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

/* Default mock server configuration */
#define E2E_DEFAULT_PORT 8765
#define E2E_DEFAULT_HOST "localhost"
#define E2E_SERVER_TIMEOUT_SEC 5
#define E2E_PYTHON_CMD "python3"

/* Mock server configuration structure */
typedef struct {
    int port;
    const char *stream_file;
    int meta_interval;
    const char *station_name;
    int bitrate;
    const char *content_type;
    int drop_after_bytes;
    const char **metadata_sequence;
    size_t metadata_count;
} MockServerConfig;

/* Initialize mock server config with defaults */
void mock_server_config_init(MockServerConfig *config);

/**
 * Start the mock streaming server.
 *
 * @param config Server configuration (NULL for defaults)
 * @return 0 on success, -1 on failure
 *
 * The server runs in the background as a separate process.
 * Use mock_server_stop() to terminate it.
 */
int mock_server_start(const MockServerConfig *config);

/**
 * Stop the mock streaming server.
 *
 * @return 0 on success, -1 on failure
 */
int mock_server_stop(void);

/**
 * Check if mock server is running and responsive.
 *
 * @return true if server responds to /status, false otherwise
 */
bool mock_server_is_running(void);

/**
 * Wait for the mock server to be ready.
 *
 * @param timeout_sec Maximum seconds to wait
 * @return true if server is ready, false if timeout
 */
bool mock_server_wait_ready(int timeout_sec);

/**
 * Get the base URL for the mock server.
 *
 * @param buffer Buffer to store URL
 * @param bufsize Size of buffer
 * @param path Optional path to append (can be NULL)
 * @return Pointer to buffer, or NULL on error
 */
char *mock_server_get_url(char *buffer, size_t bufsize, const char *path);

/* ========================================================================== */
/* File verification helpers */
/* ========================================================================== */

/**
 * Check if a file exists.
 *
 * @param path Path to file
 * @return true if file exists, false otherwise
 */
bool file_exists(const char *path);

/**
 * Get file size in bytes.
 *
 * @param path Path to file
 * @return File size, or -1 on error
 */
long file_get_size(const char *path);

/**
 * Check if file size is at least min_size bytes.
 *
 * @param path Path to file
 * @param min_size Minimum expected size
 * @return true if file size >= min_size, false otherwise
 */
bool file_size_at_least(const char *path, long min_size);

/**
 * Count files in a directory matching a pattern.
 *
 * @param dir Directory path
 * @param pattern Glob pattern (e.g., "*.mp3") or NULL for all files
 * @return Number of matching files, or -1 on error
 */
int count_files_in_dir(const char *dir, const char *pattern);

/**
 * Remove a directory and all its contents.
 *
 * @param path Directory path
 * @return 0 on success, -1 on error
 */
int remove_directory_recursive(const char *path);

/**
 * Create a temporary directory for test output.
 *
 * @param buffer Buffer to store path
 * @param bufsize Size of buffer
 * @param prefix Prefix for directory name
 * @return Pointer to buffer, or NULL on error
 */
char *create_temp_directory(char *buffer, size_t bufsize, const char *prefix);

/* ========================================================================== */
/* Test fixture helpers */
/* ========================================================================== */

/**
 * Get path to fixtures directory.
 *
 * @param buffer Buffer to store path
 * @param bufsize Size of buffer
 * @return Pointer to buffer, or NULL on error
 */
char *get_fixtures_dir(char *buffer, size_t bufsize);

/**
 * Get path to a specific fixture file.
 *
 * @param buffer Buffer to store path
 * @param bufsize Size of buffer
 * @param relative_path Path relative to fixtures dir
 * @return Pointer to buffer, or NULL on error
 */
char *get_fixture_path(char *buffer, size_t bufsize, const char *relative_path);

/* ========================================================================== */
/* Process helpers */
/* ========================================================================== */

/**
 * Execute a command and wait for completion.
 *
 * @param cmd Command to execute
 * @param timeout_sec Maximum seconds to wait (0 = no timeout)
 * @return Exit code of command, or -1 on error
 */
int execute_command(const char *cmd, int timeout_sec);

/**
 * Execute a command in background.
 *
 * @param cmd Command to execute
 * @return Process ID, or -1 on error
 */
int execute_command_background(const char *cmd);

/**
 * Kill a background process.
 *
 * @param pid Process ID from execute_command_background
 * @return 0 on success, -1 on error
 */
int kill_background_process(int pid);

/* ========================================================================== */
/* URL and PREFS helpers for E2E tests */
/* ========================================================================== */

/**
 * Initialize STREAM_PREFS with test defaults.
 *
 * @param prefs Pointer to STREAM_PREFS structure
 * @param url Stream URL
 * @param output_dir Output directory for ripped files
 */
void e2e_init_stream_prefs(void *prefs, const char *url, const char *output_dir);

/**
 * Set auto-reconnect options in STREAM_PREFS.
 *
 * @param prefs Pointer to STREAM_PREFS structure
 * @param auto_reconnect Enable auto-reconnect
 * @param timeout Timeout in seconds
 */
void e2e_set_reconnect_options(void *prefs, bool auto_reconnect, int timeout);

#endif /* TEST_E2E_HELPERS_H */
