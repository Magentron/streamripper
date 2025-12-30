/* test_e2e_helpers.c
 *
 * Implementation of E2E test helper functions.
 */

#include "test_e2e_helpers.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Include streamripper headers for STREAM_PREFS */
#include "srtypes.h"
#include "rip_manager.h"

/* Default user agent for prefs initialization */
#ifndef DEFAULT_USER_AGENT
#define DEFAULT_USER_AGENT "Streamripper/1.x"
#endif

/* ========================================================================== */
/* Stub implementations for library functions not included in E2E tests.
 * These provide minimal implementations to satisfy linker without pulling
 * in the full library dependency chain.
 * ========================================================================== */

/* Overwrite option strings */
static const char *overwrite_opt_strings[] = {
    "",        /* OVERWRITE_UNKNOWN */
    "always",  /* OVERWRITE_ALWAYS */
    "never",   /* OVERWRITE_NEVER */
    "larger",  /* OVERWRITE_LARGER */
    "version"  /* OVERWRITE_VERSION */
};

const char *overwrite_opt_to_string(enum OverwriteOpt oo)
{
    if (oo < 0 || oo > 4) return "";
    return overwrite_opt_strings[(int)oo];
}

enum OverwriteOpt string_to_overwrite_opt(char *str)
{
    if (!str) return OVERWRITE_UNKNOWN;
    for (int i = 0; i < 5; i++) {
        if (strcmp(str, overwrite_opt_strings[i]) == 0) {
            return (enum OverwriteOpt)i;
        }
    }
    return OVERWRITE_UNKNOWN;
}

/* Stub threadlib functions for debug.c */
void threadlib_create_sem(void *sem) { (void)sem; }
void threadlib_signal_sem(void *sem) { (void)sem; }
void threadlib_waitfor_sem(void *sem) { (void)sem; }

/* ========================================================================== */
/* Provide implementation of set_rip_manager_options_defaults
 * This function is declared in rip_manager.h but not implemented in the library.
 * It initializes STREAM_PREFS with sensible defaults for E2E testing.
 * ========================================================================== */
void set_rip_manager_options_defaults(STREAM_PREFS *prefs)
{
    if (!prefs) return;

    /* Clear the structure */
    memset(prefs, 0, sizeof(STREAM_PREFS));

    /* URL and proxy default to empty */
    prefs->url[0] = '\0';
    prefs->proxyurl[0] = '\0';

    /* Output settings */
    strcpy(prefs->output_directory, "./");
    prefs->output_pattern[0] = '\0';
    prefs->showfile_pattern[0] = '\0';

    /* Network interface settings */
    prefs->if_name[0] = '\0';
    prefs->relay_ip[0] = '\0';

    /* Rules and external command */
    prefs->rules_file[0] = '\0';
    prefs->pls_file[0] = '\0';
    prefs->ext_cmd[0] = '\0';

    /* Port settings */
    prefs->relay_port = 8000;
    prefs->max_port = 18000;

    /* Connection settings */
    prefs->max_connections = 1;
    prefs->maxMB_rip_size = 0;
    prefs->timeout = 15;
    prefs->dropcount = 1;  /* Changed from 0 to 1 in version 1.63-beta-8 */
    prefs->count_start = 0;

    /* Default flags:
     * - Auto-reconnect ON
     * - Separate dirs ON
     * - Search ports ON
     * - ID3V2 ON
     * - Individual tracks ON
     */
    prefs->flags = OPT_AUTO_RECONNECT | OPT_SEPARATE_DIRS | OPT_SEARCH_PORTS |
                   OPT_ADD_ID3V2 | OPT_INDIVIDUAL_TRACKS;

    /* User agent */
    strcpy(prefs->useragent, DEFAULT_USER_AGENT);

    /* Splitpoint options - times are in milliseconds */
    prefs->sp_opt.xs = 1;
    prefs->sp_opt.xs_min_volume = 1;
    prefs->sp_opt.xs_silence_length = 1000;
    prefs->sp_opt.xs_search_window_1 = 6000;
    prefs->sp_opt.xs_search_window_2 = 6000;
    prefs->sp_opt.xs_offset = 0;
    prefs->sp_opt.xs_padding_1 = 0;  /* Changed from 300 to 0 in version 1.64.5 */
    prefs->sp_opt.xs_padding_2 = 0;  /* Changed from 300 to 0 in version 1.64.5 */

    /* Codeset options - default to empty (use system defaults) */
    memset(&prefs->cs_opt, 0, sizeof(CODESET_OPTIONS));

    /* Overwrite option */
    prefs->overwrite = OVERWRITE_VERSION;

    /* HTTP/1.0 mode off by default */
    prefs->http10 = 0;

    /* WAV output off by default */
    prefs->wav_output = 0;
}

/* Global state for mock server process */
static pid_t g_mock_server_pid = -1;
static int g_mock_server_port = E2E_DEFAULT_PORT;

/* Path to mock_server.py relative to test executable */
static const char *MOCK_SERVER_SCRIPT = "mock_server.py";

/* ========================================================================== */
/* Mock server configuration */
/* ========================================================================== */

void mock_server_config_init(MockServerConfig *config)
{
    if (!config) return;

    config->port = E2E_DEFAULT_PORT;
    config->stream_file = NULL;
    config->meta_interval = 8192;
    config->station_name = "Test Radio Station";
    config->bitrate = 128;
    config->content_type = "audio/mpeg";
    config->drop_after_bytes = 0;
    config->metadata_sequence = NULL;
    config->metadata_count = 0;
}

/* ========================================================================== */
/* Mock server control */
/* ========================================================================== */

static char *find_mock_server_script(char *buffer, size_t bufsize)
{
    /* Try several locations for the mock server script */
    const char *search_paths[] = {
        /* Relative to test executable in build dir */
        "../e2e/mock_server.py",
        "../../e2e/mock_server.py",
        /* Relative to source dir */
        "tests/e2e/mock_server.py",
        "../tests/e2e/mock_server.py",
        "../../tests/e2e/mock_server.py",
        /* In same directory */
        "mock_server.py",
        NULL
    };

    for (int i = 0; search_paths[i] != NULL; i++) {
        if (access(search_paths[i], F_OK) == 0) {
            char resolved[PATH_MAX];
            if (realpath(search_paths[i], resolved)) {
                strncpy(buffer, resolved, bufsize - 1);
                buffer[bufsize - 1] = '\0';
                return buffer;
            }
        }
    }

    return NULL;
}

int mock_server_start(const MockServerConfig *config)
{
    MockServerConfig default_config;
    if (!config) {
        mock_server_config_init(&default_config);
        config = &default_config;
    }

    /* Don't start if already running */
    if (g_mock_server_pid > 0) {
        return 0;
    }

    /* Find the mock server script */
    char script_path[PATH_MAX];
    if (!find_mock_server_script(script_path, sizeof(script_path))) {
        fprintf(stderr, "E2E: Could not find mock_server.py\n");
        return -1;
    }

    g_mock_server_port = config->port;

    /* Build command line */
    char cmd[2048];
    int offset = snprintf(cmd, sizeof(cmd),
                         "%s %s --port %d --meta-interval %d --bitrate %d "
                         "--station-name '%s' --content-type '%s'",
                         E2E_PYTHON_CMD, script_path,
                         config->port, config->meta_interval, config->bitrate,
                         config->station_name, config->content_type);

    if (config->stream_file) {
        offset += snprintf(cmd + offset, sizeof(cmd) - offset,
                          " --stream '%s'", config->stream_file);
    }

    if (config->drop_after_bytes > 0) {
        offset += snprintf(cmd + offset, sizeof(cmd) - offset,
                          " --drop-after %d", config->drop_after_bytes);
    }

    /* Fork and exec the mock server */
    pid_t pid = fork();
    if (pid < 0) {
        perror("E2E: fork failed");
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        /* Redirect stdout/stderr to /dev/null unless debugging */
        if (!getenv("DEBUG")) {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
        }

        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }

    /* Parent process */
    g_mock_server_pid = pid;

    /* Wait for server to be ready */
    if (!mock_server_wait_ready(E2E_SERVER_TIMEOUT_SEC)) {
        fprintf(stderr, "E2E: Mock server failed to start within %d seconds\n",
                E2E_SERVER_TIMEOUT_SEC);
        mock_server_stop();
        return -1;
    }

    return 0;
}

int mock_server_stop(void)
{
    if (g_mock_server_pid <= 0) {
        return 0;
    }

    /* Try graceful shutdown first via HTTP */
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/shutdown",
             E2E_DEFAULT_HOST, g_mock_server_port);

    /* Use curl or wget if available */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "curl -s '%s' > /dev/null 2>&1", url);
    system(cmd);

    /* Give server time to shut down gracefully */
    usleep(100000); /* 100ms */

    /* Check if process is still running */
    int status;
    pid_t result = waitpid(g_mock_server_pid, &status, WNOHANG);

    if (result == 0) {
        /* Still running, send SIGTERM */
        kill(g_mock_server_pid, SIGTERM);
        usleep(100000);

        result = waitpid(g_mock_server_pid, &status, WNOHANG);
        if (result == 0) {
            /* Still running, send SIGKILL */
            kill(g_mock_server_pid, SIGKILL);
            waitpid(g_mock_server_pid, &status, 0);
        }
    }

    g_mock_server_pid = -1;
    return 0;
}

bool mock_server_is_running(void)
{
    if (g_mock_server_pid <= 0) {
        return false;
    }

    /* Check if process exists */
    if (kill(g_mock_server_pid, 0) != 0) {
        return false;
    }

    /* Try to connect to status endpoint */
    char url[256];
    snprintf(url, sizeof(url), "curl -s -o /dev/null -w '%%{http_code}' "
             "'http://%s:%d/status' 2>/dev/null",
             E2E_DEFAULT_HOST, g_mock_server_port);

    FILE *fp = popen(url, "r");
    if (!fp) {
        return false;
    }

    char response[16] = {0};
    if (fgets(response, sizeof(response), fp)) {
        pclose(fp);
        return strncmp(response, "200", 3) == 0;
    }

    pclose(fp);
    return false;
}

bool mock_server_wait_ready(int timeout_sec)
{
    time_t start = time(NULL);

    while (time(NULL) - start < timeout_sec) {
        if (mock_server_is_running()) {
            return true;
        }
        usleep(100000); /* 100ms */
    }

    return false;
}

char *mock_server_get_url(char *buffer, size_t bufsize, const char *path)
{
    if (!buffer || bufsize == 0) {
        return NULL;
    }

    if (path && path[0] != '/') {
        snprintf(buffer, bufsize, "http://%s:%d/%s",
                 E2E_DEFAULT_HOST, g_mock_server_port, path);
    } else if (path) {
        snprintf(buffer, bufsize, "http://%s:%d%s",
                 E2E_DEFAULT_HOST, g_mock_server_port, path);
    } else {
        snprintf(buffer, bufsize, "http://%s:%d/",
                 E2E_DEFAULT_HOST, g_mock_server_port);
    }

    return buffer;
}

/* ========================================================================== */
/* File verification helpers */
/* ========================================================================== */

bool file_exists(const char *path)
{
    if (!path) return false;
    struct stat st;
    return stat(path, &st) == 0;
}

long file_get_size(const char *path)
{
    if (!path) return -1;

    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }

    return (long)st.st_size;
}

bool file_size_at_least(const char *path, long min_size)
{
    long size = file_get_size(path);
    return size >= min_size;
}

int count_files_in_dir(const char *dir, const char *pattern)
{
    if (!dir) return -1;

    DIR *d = opendir(dir);
    if (!d) return -1;

    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(d)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Check if regular file */
        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        /* Check pattern match */
        if (pattern) {
            if (fnmatch(pattern, entry->d_name, 0) != 0) {
                continue;
            }
        }

        count++;
    }

    closedir(d);
    return count;
}

int remove_directory_recursive(const char *path)
{
    if (!path) return -1;

    DIR *d = opendir(path);
    if (!d) {
        /* Not a directory, try to remove as file */
        return unlink(path);
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(fullpath, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            remove_directory_recursive(fullpath);
        } else {
            unlink(fullpath);
        }
    }

    closedir(d);
    return rmdir(path);
}

char *create_temp_directory(char *buffer, size_t bufsize, const char *prefix)
{
    if (!buffer || bufsize == 0) return NULL;

    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";

    snprintf(buffer, bufsize, "%s/%s_XXXXXX", tmpdir, prefix ? prefix : "e2e_test");

    if (mkdtemp(buffer) == NULL) {
        return NULL;
    }

    return buffer;
}

/* ========================================================================== */
/* Test fixture helpers */
/* ========================================================================== */

char *get_fixtures_dir(char *buffer, size_t bufsize)
{
    if (!buffer || bufsize == 0) return NULL;

    /* Try several locations for the fixtures directory */
    const char *search_paths[] = {
        "../fixtures",
        "../../fixtures",
        "tests/fixtures",
        "../tests/fixtures",
        "../../tests/fixtures",
        "fixtures",
        NULL
    };

    for (int i = 0; search_paths[i] != NULL; i++) {
        struct stat st;
        if (stat(search_paths[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            char resolved[PATH_MAX];
            if (realpath(search_paths[i], resolved)) {
                strncpy(buffer, resolved, bufsize - 1);
                buffer[bufsize - 1] = '\0';
                return buffer;
            }
        }
    }

    return NULL;
}

char *get_fixture_path(char *buffer, size_t bufsize, const char *relative_path)
{
    if (!buffer || bufsize == 0 || !relative_path) return NULL;

    char fixtures_dir[PATH_MAX];
    if (!get_fixtures_dir(fixtures_dir, sizeof(fixtures_dir))) {
        return NULL;
    }

    snprintf(buffer, bufsize, "%s/%s", fixtures_dir, relative_path);
    return buffer;
}

/* ========================================================================== */
/* Process helpers */
/* ========================================================================== */

int execute_command(const char *cmd, int timeout_sec)
{
    if (!cmd) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }

    /* Wait for child with optional timeout */
    int status;
    if (timeout_sec > 0) {
        time_t start = time(NULL);
        while (time(NULL) - start < timeout_sec) {
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result > 0) {
                return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            }
            usleep(100000);
        }
        /* Timeout - kill child */
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        return -1;
    } else {
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
}

int execute_command_background(const char *cmd)
{
    if (!cmd) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        /* Daemonize */
        setsid();

        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }

    return pid;
}

int kill_background_process(int pid)
{
    if (pid <= 0) return -1;

    if (kill(pid, SIGTERM) != 0) {
        return -1;
    }

    usleep(100000);

    /* Check if still running */
    if (kill(pid, 0) == 0) {
        kill(pid, SIGKILL);
    }

    int status;
    waitpid(pid, &status, WNOHANG);

    return 0;
}

/* ========================================================================== */
/* URL and PREFS helpers for E2E tests */
/* ========================================================================== */

void e2e_init_stream_prefs(void *prefs_ptr, const char *url, const char *output_dir)
{
    STREAM_PREFS *prefs = (STREAM_PREFS *)prefs_ptr;
    if (!prefs) return;

    /* Clear structure */
    memset(prefs, 0, sizeof(STREAM_PREFS));

    /* Set URL */
    if (url) {
        strncpy(prefs->url, url, sizeof(prefs->url) - 1);
    }

    /* Set output directory */
    if (output_dir) {
        strncpy(prefs->output_directory, output_dir, sizeof(prefs->output_directory) - 1);
    }

    /* Set reasonable defaults */
    prefs->timeout = 30;
    prefs->relay_port = 8000;
    prefs->max_port = 18000;
    prefs->max_connections = 10;
    prefs->flags = OPT_INDIVIDUAL_TRACKS;

    /* Default splitpoint options */
    prefs->sp_opt.xs = 1;
    prefs->sp_opt.xs_min_volume = 1;
    prefs->sp_opt.xs_silence_length = 1000;
    prefs->sp_opt.xs_search_window_1 = 6000;
    prefs->sp_opt.xs_search_window_2 = 6000;
    prefs->sp_opt.xs_offset = 0;
    prefs->sp_opt.xs_padding_1 = 300;
    prefs->sp_opt.xs_padding_2 = 300;

    /* Default codeset options */
    strcpy(prefs->cs_opt.codeset_locale, "");
    strcpy(prefs->cs_opt.codeset_filesys, "");
    strcpy(prefs->cs_opt.codeset_id3, "ISO-8859-1");
    strcpy(prefs->cs_opt.codeset_metadata, "");
    strcpy(prefs->cs_opt.codeset_relay, "");
}

void e2e_set_reconnect_options(void *prefs_ptr, bool auto_reconnect, int timeout)
{
    STREAM_PREFS *prefs = (STREAM_PREFS *)prefs_ptr;
    if (!prefs) return;

    if (auto_reconnect) {
        prefs->flags |= OPT_AUTO_RECONNECT;
    } else {
        prefs->flags &= ~OPT_AUTO_RECONNECT;
    }

    prefs->timeout = timeout;
}
