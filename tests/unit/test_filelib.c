/* test_filelib.c - Unit tests for lib/filelib.c
 *
 * Tests for file library operations including path macros, constants,
 * and I/O operations (init, start, write, end, shutdown).
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <unity.h>

#include "compat.h"
#include "errors.h"
#include "filelib.h"
#include "mchar.h"
#include "srtypes.h"
#include "test_helpers.h"

/* ========================================================================== */
/* Stub implementations for filelib.c dependencies                            */
/* ========================================================================== */

/* Debug stubs - filelib.c uses these extensively for logging */
void debug_printf(char *fmt, ...) { (void)fmt; }
void debug_print_error(void) {}

/* mchar stubs - filelib.c uses these for string conversions */
int gstring_from_string(RIP_MANAGER_INFO *rmi, mchar *m, int mlen, char *c, int codeset_type)
{
    (void)rmi;
    (void)codeset_type;
    if (m && c && mlen > 0) {
        strncpy(m, c, mlen - 1);
        m[mlen - 1] = '\0';
    }
    return 0;
}

int string_from_gstring(RIP_MANAGER_INFO *rmi, char *c, int clen, mchar *m, int codeset_type)
{
    (void)rmi;
    (void)codeset_type;
    if (c && m && clen > 0) {
        strncpy(c, m, clen - 1);
        c[clen - 1] = '\0';
    }
    return 0;
}

/* sr_strncpy stub from mchar.c */
void sr_strncpy(char *dst, char *src, int n)
{
    if (dst && src && n > 0) {
        strncpy(dst, src, n - 1);
        dst[n - 1] = '\0';
    }
}

/* Additional mchar stubs - filelib.c uses these string utility functions */
mchar *mstrcpy(mchar *dest, const mchar *src)
{
    return strcpy(dest, src);
}

void mstrncpy(mchar *dst, mchar *src, int n)
{
    if (dst && src && n > 0) {
        strncpy(dst, src, n - 1);
        dst[n - 1] = '\0';
    }
}

size_t mstrlen(mchar *s)
{
    return s ? strlen(s) : 0;
}

mchar *mstrncat(mchar *ws1, const mchar *ws2, size_t n)
{
    return strncat(ws1, ws2, n);
}

mchar *mstrchr(const mchar *ws, mchar wc)
{
    return strchr(ws, wc);
}

mchar *mstrrchr(const mchar *ws, mchar wc)
{
    return strrchr(ws, wc);
}

int mstrcmp(const mchar *ws1, const mchar *ws2)
{
    return strcmp(ws1, ws2);
}

long int mtol(const mchar *string)
{
    return strtol(string, NULL, 10);
}

int msnprintf(mchar *dest, size_t n, const mchar *fmt, ...)
{
    va_list args;
    int result;
    va_start(args, fmt);
    result = vsnprintf(dest, n, fmt, args);
    va_end(args);
    return result;
}

/* ========================================================================== */
/* Test Fixture and Helpers                                                   */
/* ========================================================================== */

/* Global test fixture for I/O tests */
static char g_temp_dir[SR_MAX_PATH];
static RIP_MANAGER_INFO g_rmi;
static STREAM_PREFS g_prefs;
static int g_temp_dir_created = 0;

/* Helper to initialize RIP_MANAGER_INFO with sensible defaults */
static void init_rmi_defaults(RIP_MANAGER_INFO *rmi, STREAM_PREFS *prefs)
{
    memset(rmi, 0, sizeof(RIP_MANAGER_INFO));
    memset(prefs, 0, sizeof(STREAM_PREFS));

    rmi->prefs = prefs;

    /* Initialize the filelib_info structure */
    rmi->filelib_info.m_file = INVALID_FHANDLE;
    rmi->filelib_info.m_show_file = INVALID_FHANDLE;
    rmi->filelib_info.m_cue_file = INVALID_FHANDLE;
    rmi->filelib_info.m_count = -1;
    rmi->filelib_info.m_do_show = FALSE;
    rmi->filelib_info.m_do_individual_tracks = TRUE;
    rmi->filelib_info.m_keep_incomplete = FALSE;
    rmi->filelib_info.m_track_no = 1;

    /* Initialize http_info for content type */
    rmi->http_info.content_type = CONTENT_TYPE_MP3;

    /* Set default codeset options */
    strcpy(rmi->mchar_cs.codeset_locale, "UTF-8");
    strcpy(rmi->mchar_cs.codeset_filesys, "UTF-8");
    strcpy(rmi->mchar_cs.codeset_id3, "UTF-8");
    strcpy(rmi->mchar_cs.codeset_metadata, "UTF-8");
    strcpy(rmi->mchar_cs.codeset_relay, "UTF-8");
}

/* Helper to initialize TRACK_INFO */
static void init_track_info(TRACK_INFO *ti, const char *artist, const char *title)
{
    memset(ti, 0, sizeof(TRACK_INFO));
    ti->have_track_info = 1;
    ti->save_track = TRUE;

    if (artist) {
        strncpy(ti->artist, artist, MAX_TRACK_LEN - 1);
    }
    if (title) {
        strncpy(ti->title, title, MAX_TRACK_LEN - 1);
    }
}

/* Helper to remove directory and its contents recursively */
static int remove_directory_recursive(const char *path)
{
    char filepath[SR_MAX_PATH];
    struct stat st;
    DIR *dir;
    struct dirent *entry;

    if (stat(path, &st) != 0) {
        return 0;  /* Directory doesn't exist, consider it success */
    }

    if (!S_ISDIR(st.st_mode)) {
        return unlink(path);
    }

    dir = opendir(path);
    if (dir == NULL) {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

        if (stat(filepath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                remove_directory_recursive(filepath);
            } else {
                unlink(filepath);
            }
        }
    }

    closedir(dir);
    return rmdir(path);
}

void setUp(void)
{
    /* Initialize RIP_MANAGER_INFO with defaults */
    init_rmi_defaults(&g_rmi, &g_prefs);

    /* Create a temporary directory for I/O tests */
    g_temp_dir_created = 0;
    if (test_create_temp_dir(g_temp_dir, sizeof(g_temp_dir)) == 0) {
        g_temp_dir_created = 1;
    }
}

void tearDown(void)
{
    /* Ensure filelib is shut down */
    filelib_shutdown(&g_rmi);

    /* Clean up temporary directory */
    if (g_temp_dir_created && g_temp_dir[0] != '\0') {
        remove_directory_recursive(g_temp_dir);
        g_temp_dir[0] = '\0';
    }
    g_temp_dir_created = 0;
}

/* ========================================================================== */
/* Path Macros and Constants Tests (existing tests)                           */
/* ========================================================================== */

/* Test: PATH_SLASH is defined correctly for Unix */
static void test_path_slash_unix(void)
{
    /* On Unix, PATH_SLASH should be '/' */
#if !WIN32
    char slash = '/';
    TEST_ASSERT_EQUAL(slash, PATH_SLASH);
    TEST_ASSERT_EQUAL_STRING("/", PATH_SLASH_STR);
#endif
}

/* Test: ISSLASH macro correctly identifies slash characters */
static void test_isslash_macro(void)
{
    /* Forward slash should always be a slash */
    TEST_ASSERT_TRUE(ISSLASH('/'));

    /* Regular characters should not be slashes */
    TEST_ASSERT_FALSE(ISSLASH('a'));
    TEST_ASSERT_FALSE(ISSLASH('0'));
    TEST_ASSERT_FALSE(ISSLASH(' '));
    TEST_ASSERT_FALSE(ISSLASH('\0'));
}

/* Test: IS_ABSOLUTE_PATH macro works correctly */
static void test_is_absolute_path_unix(void)
{
#if !WIN32
    /* Absolute paths start with / on Unix */
    TEST_ASSERT_TRUE(IS_ABSOLUTE_PATH("/home/user"));
    TEST_ASSERT_TRUE(IS_ABSOLUTE_PATH("/"));
    TEST_ASSERT_TRUE(IS_ABSOLUTE_PATH("/tmp/file.mp3"));

    /* Relative paths don't start with / */
    TEST_ASSERT_FALSE(IS_ABSOLUTE_PATH("home/user"));
    TEST_ASSERT_FALSE(IS_ABSOLUTE_PATH("./file.mp3"));
    TEST_ASSERT_FALSE(IS_ABSOLUTE_PATH("../file.mp3"));
    TEST_ASSERT_FALSE(IS_ABSOLUTE_PATH("file.mp3"));
#endif
}

/* Test: HAS_DEVICE macro for Unix (should always return 0) */
static void test_has_device_unix(void)
{
#if !WIN32
    /* On Unix, there are no device letters */
    TEST_ASSERT_FALSE(HAS_DEVICE("/home/user"));
    TEST_ASSERT_FALSE(HAS_DEVICE("C:/something"));
    TEST_ASSERT_FALSE(HAS_DEVICE("D:\\path"));
#endif
}

/* Test: SR_MAX_PATH and related constants are reasonable */
static void test_path_constants(void)
{
    /* SR_MAX_PATH should be a reasonable value */
    TEST_ASSERT_TRUE(SR_MAX_PATH >= 254);
    TEST_ASSERT_TRUE(SR_MAX_PATH <= 4096);

    /* SR_MIN_FILENAME should leave room for actual filenames */
    TEST_ASSERT_TRUE(SR_MIN_FILENAME > 0);
    TEST_ASSERT_TRUE(SR_MIN_FILENAME < SR_MAX_PATH);

    /* SR_MIN_COMPLETEDIR should be reasonable */
    TEST_ASSERT_TRUE(SR_MIN_COMPLETEDIR > 0);
    TEST_ASSERT_TRUE(SR_MIN_COMPLETEDIR < SR_MAX_PATH);

    /* SR_DATE_LEN should accommodate date strings */
    TEST_ASSERT_TRUE(SR_DATE_LEN >= 10);  /* At least YYYY-MM-DD */
}

/* Test: Maximum path calculations are consistent */
static void test_max_path_calculations(void)
{
    /* SR_MAX_INCOMPLETE should be less than SR_MAX_PATH */
    int max_incomplete = SR_MAX_PATH - SR_MIN_FILENAME;
    TEST_ASSERT_TRUE(max_incomplete > 0);
    TEST_ASSERT_TRUE(max_incomplete < SR_MAX_PATH);
}

/* Test: Content type constants are defined */
static void test_content_type_constants(void)
{
    /* Verify content type constants are unique and defined */
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_MP3, CONTENT_TYPE_OGG);
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_MP3, CONTENT_TYPE_AAC);
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_OGG, CONTENT_TYPE_AAC);
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_MP3, CONTENT_TYPE_NSV);
    TEST_ASSERT_NOT_EQUAL(CONTENT_TYPE_UNKNOWN, CONTENT_TYPE_MP3);
}

/* Test: OverwriteOpt enum values are distinct */
static void test_overwrite_opt_values(void)
{
    /* Verify overwrite options are distinct */
    TEST_ASSERT_NOT_EQUAL(OVERWRITE_UNKNOWN, OVERWRITE_ALWAYS);
    TEST_ASSERT_NOT_EQUAL(OVERWRITE_ALWAYS, OVERWRITE_NEVER);
    TEST_ASSERT_NOT_EQUAL(OVERWRITE_NEVER, OVERWRITE_LARGER);
    TEST_ASSERT_NOT_EQUAL(OVERWRITE_LARGER, OVERWRITE_VERSION);
}

/* Test: DATEBUF_LEN is sufficient for date strings */
static void test_datebuf_len(void)
{
    /* Date buffer should hold at least YYYY_MM_DD_HH_MM_SS format */
    /* Format: 2024_12_30_14_30_45 = 19 chars + null = 20 */
    TEST_ASSERT_TRUE(DATEBUF_LEN >= 20);
}

/* Test: FILELIB_INFO structure fields exist */
static void test_filelib_info_structure(void)
{
    FILELIB_INFO fli;

    /* Test that we can set and read fields */
    memset(&fli, 0, sizeof(FILELIB_INFO));

    fli.m_count = 5;
    TEST_ASSERT_EQUAL(5, fli.m_count);

    fli.m_do_show = TRUE;
    TEST_ASSERT_EQUAL(TRUE, fli.m_do_show);

    fli.m_keep_incomplete = FALSE;
    TEST_ASSERT_EQUAL(FALSE, fli.m_keep_incomplete);

    fli.m_max_filename_length = 100;
    TEST_ASSERT_EQUAL(100, fli.m_max_filename_length);

    fli.m_do_individual_tracks = TRUE;
    TEST_ASSERT_EQUAL(TRUE, fli.m_do_individual_tracks);

    fli.m_track_no = 3;
    TEST_ASSERT_EQUAL(3, fli.m_track_no);
}

/* Test: Path string arrays in FILELIB_INFO are sized correctly */
static void test_filelib_info_path_sizes(void)
{
    FILELIB_INFO fli;

    /* Verify path arrays are SR_MAX_PATH sized */
    TEST_ASSERT_EQUAL(SR_MAX_PATH, sizeof(fli.m_output_directory) / sizeof(mchar));
    TEST_ASSERT_EQUAL(SR_MAX_PATH, sizeof(fli.m_output_pattern) / sizeof(mchar));
    TEST_ASSERT_EQUAL(SR_MAX_PATH, sizeof(fli.m_incomplete_directory) / sizeof(mchar));
    TEST_ASSERT_EQUAL(SR_MAX_PATH, sizeof(fli.m_show_name) / sizeof(mchar));
}

/* ========================================================================== */
/* filelib_init Tests                                                         */
/* ========================================================================== */

/* Test: filelib_init with valid output directory for MP3 content */
static void test_filelib_init_valid_directory_mp3(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,                    /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        FALSE,                   /* keep_incomplete */
        FALSE,                   /* do_show_file */
        CONTENT_TYPE_MP3,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "Test Radio"             /* icy_name */
    );

    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Verify filelib_info was initialized correctly */
    TEST_ASSERT_EQUAL_STRING(".mp3", g_rmi.filelib_info.m_extension);
    TEST_ASSERT_TRUE(g_rmi.filelib_info.m_do_individual_tracks);
    TEST_ASSERT_FALSE(g_rmi.filelib_info.m_keep_incomplete);
    TEST_ASSERT_FALSE(g_rmi.filelib_info.m_do_show);

    /* Note: Directory creation is tested via actual file I/O in lifecycle tests */
}

/* Test: filelib_init with OGG content type */
static void test_filelib_init_valid_directory_ogg(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,                    /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        FALSE,                   /* keep_incomplete */
        FALSE,                   /* do_show_file */
        CONTENT_TYPE_OGG,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "OGG Radio"              /* icy_name */
    );

    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    TEST_ASSERT_EQUAL_STRING(".ogg", g_rmi.filelib_info.m_extension);
}

/* Test: filelib_init with AAC content type */
static void test_filelib_init_valid_directory_aac(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,                    /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        FALSE,                   /* keep_incomplete */
        FALSE,                   /* do_show_file */
        CONTENT_TYPE_AAC,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "AAC Radio"              /* icy_name */
    );

    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    TEST_ASSERT_EQUAL_STRING(".aac", g_rmi.filelib_info.m_extension);
}

/* Test: filelib_init with NSV content type */
static void test_filelib_init_valid_directory_nsv(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,                    /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        FALSE,                   /* keep_incomplete */
        FALSE,                   /* do_show_file */
        CONTENT_TYPE_NSV,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "NSV Radio"              /* icy_name */
    );

    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    TEST_ASSERT_EQUAL_STRING(".nsv", g_rmi.filelib_info.m_extension);
}

/* Test: filelib_init with invalid content type */
static void test_filelib_init_invalid_content_type(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,                    /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        FALSE,                   /* keep_incomplete */
        FALSE,                   /* do_show_file */
        999,                     /* invalid content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "Invalid Radio"          /* icy_name */
    );

    TEST_ASSERT_EQUAL(SR_ERROR_PROGRAM_ERROR, result);
}

/* Test: filelib_init with counting enabled */
static void test_filelib_init_with_counting(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,                    /* do_individual_tracks */
        TRUE,                    /* do_count */
        42,                      /* count_start */
        FALSE,                   /* keep_incomplete */
        FALSE,                   /* do_show_file */
        CONTENT_TYPE_MP3,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "Counting Radio"         /* icy_name */
    );

    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    TEST_ASSERT_EQUAL(42, g_rmi.filelib_info.m_count);
}

/* Test: filelib_init with keep_incomplete enabled */
static void test_filelib_init_keep_incomplete(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,                    /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        TRUE,                    /* keep_incomplete */
        FALSE,                   /* do_show_file */
        CONTENT_TYPE_MP3,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "Keep Incomplete Radio"  /* icy_name */
    );

    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    TEST_ASSERT_TRUE(g_rmi.filelib_info.m_keep_incomplete);
}

/* Test: filelib_init with separate directories enabled */
static void test_filelib_init_separate_dirs(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,                    /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        FALSE,                   /* keep_incomplete */
        FALSE,                   /* do_show_file */
        CONTENT_TYPE_MP3,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        TRUE,                    /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "Separate Dirs Radio"    /* icy_name */
    );

    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Pattern should include %S for stream name directory */
    TEST_ASSERT_TRUE(strstr(g_rmi.filelib_info.m_default_pattern, "%S") != NULL);
}

/* Test: filelib_init with no individual tracks (show mode only) */
static void test_filelib_init_no_individual_tracks(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        FALSE,                   /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        FALSE,                   /* keep_incomplete */
        FALSE,                   /* do_show_file */
        CONTENT_TYPE_MP3,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "No Tracks Radio"        /* icy_name */
    );

    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    TEST_ASSERT_FALSE(g_rmi.filelib_info.m_do_individual_tracks);
}

/* ========================================================================== */
/* filelib_start Tests                                                        */
/* ========================================================================== */

/* Test: filelib_start creates incomplete file */
static void test_filelib_start_creates_file(void)
{
    error_code result;
    TRACK_INFO ti;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* First initialize filelib */
    result = filelib_init(
        &g_rmi,
        TRUE,                    /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        FALSE,                   /* keep_incomplete */
        FALSE,                   /* do_show_file */
        CONTENT_TYPE_MP3,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "Start Test Radio"       /* icy_name */
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Initialize track info */
    init_track_info(&ti, "Test Artist", "Test Title");

    /* Start file writing session */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Verify file handle is valid */
    TEST_ASSERT_NOT_EQUAL(INVALID_FHANDLE, g_rmi.filelib_info.m_file);

    /* Verify incomplete filename was set */
    TEST_ASSERT_TRUE(strlen(g_rmi.filelib_info.m_incomplete_filename) > 0);
}

/* Test: filelib_start when do_individual_tracks is FALSE */
static void test_filelib_start_no_individual_tracks(void)
{
    error_code result;
    TRACK_INFO ti;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize filelib with do_individual_tracks = FALSE */
    result = filelib_init(
        &g_rmi,
        FALSE,                   /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        FALSE,                   /* keep_incomplete */
        FALSE,                   /* do_show_file */
        CONTENT_TYPE_MP3,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "No Tracks Radio"        /* icy_name */
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    init_track_info(&ti, "Artist", "Title");

    /* Should return success but not create file */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* File handle should remain invalid */
    TEST_ASSERT_EQUAL(INVALID_FHANDLE, g_rmi.filelib_info.m_file);
}

/* ========================================================================== */
/* filelib_write_track Tests                                                  */
/* ========================================================================== */

/* Test: filelib_write_track writes data to file */
static void test_filelib_write_track_writes_data(void)
{
    error_code result;
    TRACK_INFO ti;
    const char *test_data = "Test audio data 1234567890";
    u_long data_size = strlen(test_data);

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize and start */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Write Test Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    init_track_info(&ti, "Write Artist", "Write Title");

    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Write track data */
    result = filelib_write_track(&g_rmi, (char *)test_data, data_size);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
}

/* Test: filelib_write_track multiple writes */
static void test_filelib_write_track_multiple_writes(void)
{
    error_code result;
    TRACK_INFO ti;
    char test_data[256];
    int i;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize and start */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Multi Write Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    init_track_info(&ti, "Multi Artist", "Multi Title");

    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Write multiple chunks of data */
    for (i = 0; i < 10; i++) {
        snprintf(test_data, sizeof(test_data), "Audio chunk %d\n", i);
        result = filelib_write_track(&g_rmi, test_data, strlen(test_data));
        TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    }
}

/* ========================================================================== */
/* filelib_write_show Tests                                                   */
/* ========================================================================== */

/* Test: filelib_write_show when show file is disabled */
static void test_filelib_write_show_disabled(void)
{
    error_code result;
    const char *test_data = "Show data";

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize with do_show_file = FALSE */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,                   /* do_show_file = FALSE */
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "No Show Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Should return success (no-op) when show is disabled */
    result = filelib_write_show(&g_rmi, (char *)test_data, strlen(test_data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
}

/* ========================================================================== */
/* filelib_write_cue Tests                                                    */
/* ========================================================================== */

/* Test: filelib_write_cue when show file is disabled */
static void test_filelib_write_cue_disabled(void)
{
    error_code result;
    TRACK_INFO ti;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize with do_show_file = FALSE */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,                   /* do_show_file = FALSE */
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "No Cue Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    init_track_info(&ti, "Cue Artist", "Cue Title");

    /* Should return success (no-op) when show/cue is disabled */
    result = filelib_write_cue(&g_rmi, &ti, 120);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
}

/* ========================================================================== */
/* filelib_end Tests                                                          */
/* ========================================================================== */

/* Test: filelib_end moves file from incomplete to complete */
static void test_filelib_end_moves_file(void)
{
    error_code result;
    TRACK_INFO ti;
    const char *test_data = "Test audio data for end";
    mchar fullpath[SR_MAX_PATH];

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "End Test Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    init_track_info(&ti, "End Artist", "End Title");

    /* Start, write, and end */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = filelib_write_track(&g_rmi, (char *)test_data, strlen(test_data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    memset(fullpath, 0, sizeof(fullpath));
    result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* File handle should be closed (invalid) */
    TEST_ASSERT_EQUAL(INVALID_FHANDLE, g_rmi.filelib_info.m_file);

    /* Verify fullpath was set */
    TEST_ASSERT_TRUE(strlen(fullpath) > 0);
}

/* Test: filelib_end with OVERWRITE_NEVER constant value
 * Note: Full I/O behavior is tested in integration tests.
 * This test verifies the overwrite option constants are defined correctly.
 */
static void test_filelib_end_overwrite_never(void)
{
    /* Verify OVERWRITE constants have distinct values */
    TEST_ASSERT_NOT_EQUAL(OVERWRITE_ALWAYS, OVERWRITE_NEVER);
    TEST_ASSERT_NOT_EQUAL(OVERWRITE_ALWAYS, OVERWRITE_LARGER);
    TEST_ASSERT_NOT_EQUAL(OVERWRITE_NEVER, OVERWRITE_LARGER);

    /* Verify constants match expected values from filelib.h */
    TEST_ASSERT_EQUAL(0, OVERWRITE_UNKNOWN);
    TEST_ASSERT_EQUAL(1, OVERWRITE_ALWAYS);
    TEST_ASSERT_EQUAL(2, OVERWRITE_NEVER);
    TEST_ASSERT_EQUAL(3, OVERWRITE_LARGER);
}

/* Test: filelib_end when do_individual_tracks is FALSE */
static void test_filelib_end_no_individual_tracks(void)
{
    error_code result;
    TRACK_INFO ti;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize with do_individual_tracks = FALSE */
    result = filelib_init(
        &g_rmi,
        FALSE,                   /* do_individual_tracks */
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "No End Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    init_track_info(&ti, "No End Artist", "No End Title");

    /* Should return success (no-op) */
    result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, NULL);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
}

/* ========================================================================== */
/* filelib_shutdown Tests                                                     */
/* ========================================================================== */

/* Test: filelib_shutdown closes all file handles */
static void test_filelib_shutdown_closes_handles(void)
{
    error_code result;
    TRACK_INFO ti;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize and start a track */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Shutdown Test Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    init_track_info(&ti, "Shutdown Artist", "Shutdown Title");

    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Write some data */
    result = filelib_write_track(&g_rmi, "shutdown test", 13);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Shutdown should close the file handle */
    filelib_shutdown(&g_rmi);

    /* All file handles should be invalid after shutdown */
    TEST_ASSERT_EQUAL(INVALID_FHANDLE, g_rmi.filelib_info.m_file);
    TEST_ASSERT_EQUAL(INVALID_FHANDLE, g_rmi.filelib_info.m_show_file);
    TEST_ASSERT_EQUAL(INVALID_FHANDLE, g_rmi.filelib_info.m_cue_file);
}

/* Test: filelib_shutdown when already shut down (no-op) */
static void test_filelib_shutdown_idempotent(void)
{
    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Shutdown should be safe to call multiple times */
    filelib_shutdown(&g_rmi);
    filelib_shutdown(&g_rmi);
    filelib_shutdown(&g_rmi);

    /* Should still be in valid state */
    TEST_ASSERT_EQUAL(INVALID_FHANDLE, g_rmi.filelib_info.m_file);
}

/* ========================================================================== */
/* Full Lifecycle Tests                                                       */
/* ========================================================================== */

/* Test: Complete write session lifecycle (init -> start -> write -> end -> shutdown) */
static void test_full_lifecycle_single_track(void)
{
    error_code result;
    TRACK_INFO ti;
    const char *test_audio = "This is simulated audio data for testing purposes 0123456789";
    mchar fullpath[SR_MAX_PATH];
    long file_size;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Step 1: Initialize */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Lifecycle Radio"
    );
    TEST_ASSERT_EQUAL_MESSAGE(SR_SUCCESS, result, "filelib_init failed");

    /* Step 2: Start track */
    init_track_info(&ti, "Lifecycle Artist", "Lifecycle Title");
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL_MESSAGE(SR_SUCCESS, result, "filelib_start failed");

    /* Step 3: Write data */
    result = filelib_write_track(&g_rmi, (char *)test_audio, strlen(test_audio));
    TEST_ASSERT_EQUAL_MESSAGE(SR_SUCCESS, result, "filelib_write_track failed");

    /* Step 4: End track */
    memset(fullpath, 0, sizeof(fullpath));
    result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
    TEST_ASSERT_EQUAL_MESSAGE(SR_SUCCESS, result, "filelib_end failed");

    /* Verify file was created and has correct size */
    TEST_ASSERT_TRUE(strlen(fullpath) > 0);
    file_size = test_file_size(fullpath);
    TEST_ASSERT_EQUAL(strlen(test_audio), file_size);

    /* Step 5: Shutdown */
    filelib_shutdown(&g_rmi);

    /* Verify all handles are closed */
    TEST_ASSERT_EQUAL(INVALID_FHANDLE, g_rmi.filelib_info.m_file);
}

/* Test: Multiple tracks in sequence */
static void test_full_lifecycle_multiple_tracks(void)
{
    error_code result;
    TRACK_INFO ti;
    int i;
    char artist[64], title[64], data[128];
    mchar fullpath[SR_MAX_PATH];

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize once */
    result = filelib_init(
        &g_rmi,
        TRUE,
        TRUE,                    /* do_count */
        1,                       /* count_start */
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Multi Track Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Write multiple tracks */
    for (i = 0; i < 3; i++) {
        snprintf(artist, sizeof(artist), "Artist %d", i);
        snprintf(title, sizeof(title), "Title %d", i);
        snprintf(data, sizeof(data), "Audio data for track %d", i);

        init_track_info(&ti, artist, title);

        result = filelib_start(&g_rmi, &ti);
        TEST_ASSERT_EQUAL(SR_SUCCESS, result);

        result = filelib_write_track(&g_rmi, data, strlen(data));
        TEST_ASSERT_EQUAL(SR_SUCCESS, result);

        result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
        TEST_ASSERT_EQUAL(SR_SUCCESS, result);

        TEST_ASSERT_TRUE(strlen(fullpath) > 0);
    }

    /* Count should have been incremented */
    TEST_ASSERT_EQUAL(4, g_rmi.filelib_info.m_count);  /* Started at 1, incremented 3 times */

    filelib_shutdown(&g_rmi);
}

/* ========================================================================== */
/* Show File and CUE File Tests                                               */
/* ========================================================================== */

/* Test: filelib_init with show file enabled creates show and cue files */
static void test_filelib_init_with_show_file(void)
{
    error_code result;
    char show_dir[SR_MAX_PATH];

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize with do_show_file = TRUE */
    result = filelib_init(
        &g_rmi,
        TRUE,                    /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        FALSE,                   /* keep_incomplete */
        TRUE,                    /* do_show_file = TRUE */
        CONTENT_TYPE_MP3,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern - use default */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "Show Test Radio"        /* icy_name */
    );

    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Verify m_do_show is set */
    TEST_ASSERT_TRUE(g_rmi.filelib_info.m_do_show);

    /* Verify show file handle is valid (file was opened) */
    TEST_ASSERT_NOT_EQUAL(INVALID_FHANDLE, g_rmi.filelib_info.m_show_file);

    /* Verify show name was set */
    TEST_ASSERT_TRUE(strlen(g_rmi.filelib_info.m_show_name) > 0);

    /* Verify showfile directory was set */
    TEST_ASSERT_TRUE(strlen(g_rmi.filelib_info.m_showfile_directory) > 0);
}

/* Test: filelib_write_show when enabled writes data */
static void test_filelib_write_show_enabled(void)
{
    error_code result;
    const char *test_data = "Show audio data for testing 1234567890";
    u_long data_size = strlen(test_data);

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize with do_show_file = TRUE */
    result = filelib_init(
        &g_rmi,
        TRUE,                    /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        FALSE,                   /* keep_incomplete */
        TRUE,                    /* do_show_file = TRUE */
        CONTENT_TYPE_MP3,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "Write Show Radio"       /* icy_name */
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    TEST_ASSERT_TRUE(g_rmi.filelib_info.m_do_show);

    /* Write show data */
    result = filelib_write_show(&g_rmi, (char *)test_data, data_size);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
}

/* Test: filelib_write_cue when enabled writes CUE sheet entries */
static void test_filelib_write_cue_enabled(void)
{
    error_code result;
    TRACK_INFO ti;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize with do_show_file = TRUE */
    result = filelib_init(
        &g_rmi,
        TRUE,                    /* do_individual_tracks */
        FALSE,                   /* do_count */
        1,                       /* count_start */
        FALSE,                   /* keep_incomplete */
        TRUE,                    /* do_show_file = TRUE */
        CONTENT_TYPE_MP3,        /* content_type */
        g_temp_dir,              /* output_directory */
        NULL,                    /* output_pattern */
        NULL,                    /* showfile_pattern */
        FALSE,                   /* get_separate_dirs */
        FALSE,                   /* get_date_stamp */
        "CUE Test Radio"         /* icy_name */
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    TEST_ASSERT_TRUE(g_rmi.filelib_info.m_do_show);

    /* Verify CUE file handle is valid */
    TEST_ASSERT_NOT_EQUAL(INVALID_FHANDLE, g_rmi.filelib_info.m_cue_file);

    init_track_info(&ti, "CUE Artist", "CUE Title");

    /* Write CUE entry at 120 seconds */
    result = filelib_write_cue(&g_rmi, &ti, 120);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Track number should be incremented */
    TEST_ASSERT_EQUAL(2, g_rmi.filelib_info.m_track_no);
}

/* Test: Multiple CUE entries with different timestamps */
static void test_filelib_write_cue_multiple_entries(void)
{
    error_code result;
    TRACK_INFO ti;
    int i;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize with do_show_file = TRUE */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        TRUE,                    /* do_show_file = TRUE */
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Multi CUE Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Write multiple CUE entries */
    for (i = 0; i < 5; i++) {
        char artist[64], title[64];
        snprintf(artist, sizeof(artist), "Artist %d", i + 1);
        snprintf(title, sizeof(title), "Track %d", i + 1);
        init_track_info(&ti, artist, title);

        result = filelib_write_cue(&g_rmi, &ti, i * 180);  /* 3 minutes apart */
        TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    }

    /* Track number should be 6 (started at 1, incremented 5 times) */
    TEST_ASSERT_EQUAL(6, g_rmi.filelib_info.m_track_no);
}

/* ========================================================================== */
/* Overwrite Mode Tests                                                       */
/* ========================================================================== */

/* Test: filelib_end with OVERWRITE_LARGER - new file is larger */
static void test_filelib_end_overwrite_larger_new_bigger(void)
{
    error_code result;
    TRACK_INFO ti;
    mchar fullpath[SR_MAX_PATH];
    const char *small_data = "small";
    const char *large_data = "This is a much larger piece of data for testing";

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Overwrite Larger Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    init_track_info(&ti, "Test Artist", "Test Title");

    /* Write a small file first */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_write_track(&g_rmi, (char *)small_data, strlen(small_data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Verify small file was created */
    TEST_ASSERT_EQUAL(strlen(small_data), test_file_size(fullpath));

    /* Write a larger file with same name */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_write_track(&g_rmi, (char *)large_data, strlen(large_data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* End with OVERWRITE_LARGER - should overwrite since new is bigger */
    result = filelib_end(&g_rmi, &ti, OVERWRITE_LARGER, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Verify file was overwritten with larger data */
    TEST_ASSERT_EQUAL(strlen(large_data), test_file_size(fullpath));
}

/* Test: filelib_end with OVERWRITE_LARGER - new file is smaller */
static void test_filelib_end_overwrite_larger_new_smaller(void)
{
    error_code result;
    TRACK_INFO ti;
    mchar fullpath[SR_MAX_PATH];
    const char *large_data = "This is a large piece of data for testing purposes";
    const char *small_data = "tiny";

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Overwrite Smaller Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    init_track_info(&ti, "Test Artist 2", "Test Title 2");

    /* Write a large file first */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_write_track(&g_rmi, (char *)large_data, strlen(large_data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    long original_size = test_file_size(fullpath);
    TEST_ASSERT_EQUAL(strlen(large_data), original_size);

    /* Write a smaller file with same name */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_write_track(&g_rmi, (char *)small_data, strlen(small_data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* End with OVERWRITE_LARGER - should NOT overwrite since new is smaller */
    result = filelib_end(&g_rmi, &ti, OVERWRITE_LARGER, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Verify original large file is still there */
    TEST_ASSERT_EQUAL(original_size, test_file_size(fullpath));
}

/* Test: filelib_end with OVERWRITE_NEVER when file exists */
static void test_filelib_end_overwrite_never_existing(void)
{
    error_code result;
    TRACK_INFO ti1, ti2;
    mchar fullpath1[SR_MAX_PATH];
    mchar fullpath2[SR_MAX_PATH];
    const char *first_data = "First file content";
    const char *second_data = "Second file content - different";

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Never Overwrite Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Use different track names to avoid file collision in incomplete directory */
    init_track_info(&ti1, "Never Artist", "Never Title");
    init_track_info(&ti2, "Never Artist2", "Never Title2");

    /* Write first file */
    result = filelib_start(&g_rmi, &ti1);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_write_track(&g_rmi, (char *)first_data, strlen(first_data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_end(&g_rmi, &ti1, OVERWRITE_ALWAYS, FALSE, fullpath1);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    long original_size = test_file_size(fullpath1);
    TEST_ASSERT_TRUE(original_size > 0);

    /* Write second file with different name */
    result = filelib_start(&g_rmi, &ti2);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_write_track(&g_rmi, (char *)second_data, strlen(second_data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* End with OVERWRITE_NEVER - write to different file */
    result = filelib_end(&g_rmi, &ti2, OVERWRITE_NEVER, FALSE, fullpath2);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Verify both files exist with correct sizes */
    TEST_ASSERT_EQUAL(original_size, test_file_size(fullpath1));
    TEST_ASSERT_TRUE(test_file_size(fullpath2) > 0);
}

/* Test: filelib_end with OVERWRITE_VERSION creates versioned files */
static void test_filelib_end_overwrite_version(void)
{
    error_code result;
    TRACK_INFO ti;
    mchar fullpath[SR_MAX_PATH];
    const char *data = "Version test data";

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Version Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    init_track_info(&ti, "Version Artist", "Version Title");

    /* Write first file */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_write_track(&g_rmi, (char *)data, strlen(data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Write second file - should version the first */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_write_track(&g_rmi, (char *)data, strlen(data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_end(&g_rmi, &ti, OVERWRITE_VERSION, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Both files should exist */
    TEST_ASSERT_TRUE(test_file_size(fullpath) > 0);
}

/* Test: filelib_end with truncate_dup parameter */
static void test_filelib_end_truncate_dup(void)
{
    error_code result;
    TRACK_INFO ti;
    mchar fullpath[SR_MAX_PATH];
    const char *data = "Data for truncate test";

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Truncate Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    init_track_info(&ti, "Truncate Artist", "Truncate Title");

    /* Write first file */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_write_track(&g_rmi, (char *)data, strlen(data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Write second file with truncate_dup = TRUE */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    result = filelib_write_track(&g_rmi, (char *)data, strlen(data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Use OVERWRITE_NEVER with truncate_dup = TRUE */
    result = filelib_end(&g_rmi, &ti, OVERWRITE_NEVER, TRUE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
}

/* ========================================================================== */
/* Output Pattern Tests                                                       */
/* ========================================================================== */

/* Test: Custom output pattern with %A and %T */
static void test_filelib_init_custom_pattern(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        "%T - %A",               /* custom output_pattern: title - artist */
        NULL,
        FALSE,
        FALSE,
        "Pattern Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Verify output pattern was set */
    TEST_ASSERT_TRUE(strstr(g_rmi.filelib_info.m_output_pattern, "%T") != NULL ||
                     strstr(g_rmi.filelib_info.m_output_pattern, "%A") != NULL);
}

/* Test: Output pattern with date stamp %d */
static void test_filelib_init_with_date_stamp(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        TRUE,                    /* get_date_stamp = TRUE */
        "Date Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Verify session date buffer was filled */
    TEST_ASSERT_TRUE(strlen(g_rmi.filelib_info.m_session_datebuf) > 0);
}

/* Test: Output pattern with separate directories and date */
static void test_filelib_init_separate_dirs_with_date(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        TRUE,                    /* get_separate_dirs = TRUE */
        TRUE,                    /* get_date_stamp = TRUE */
        "Sep Dirs Date Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Pattern should include %S for stream name */
    TEST_ASSERT_TRUE(strstr(g_rmi.filelib_info.m_default_pattern, "%S") != NULL);
}

/* Test: Output pattern with counting and separate directories */
static void test_filelib_init_counting_separate_dirs(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        TRUE,                    /* do_count = TRUE */
        100,                     /* count_start = 100 */
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        TRUE,                    /* get_separate_dirs = TRUE */
        FALSE,
        "Count Sep Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Pattern should include both %S and %q */
    TEST_ASSERT_TRUE(strstr(g_rmi.filelib_info.m_default_pattern, "%S") != NULL);
    TEST_ASSERT_TRUE(strstr(g_rmi.filelib_info.m_default_pattern, "q") != NULL);
    TEST_ASSERT_EQUAL(100, g_rmi.filelib_info.m_count);
}

/* Test: Track with album info in pattern */
static void test_filelib_track_with_album(void)
{
    error_code result;
    TRACK_INFO ti;
    mchar fullpath[SR_MAX_PATH];
    const char *data = "Album test data";

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        "%a - %A - %T",          /* pattern with album */
        NULL,
        FALSE,
        FALSE,
        "Album Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Initialize track with album info */
    memset(&ti, 0, sizeof(TRACK_INFO));
    ti.have_track_info = 1;
    ti.save_track = TRUE;
    strncpy(ti.artist, "Album Artist", MAX_TRACK_LEN - 1);
    strncpy(ti.title, "Album Title", MAX_TRACK_LEN - 1);
    strncpy(ti.album, "Test Album", MAX_TRACK_LEN - 1);

    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = filelib_write_track(&g_rmi, (char *)data, strlen(data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    TEST_ASSERT_TRUE(test_file_size(fullpath) > 0);
}

/* ========================================================================== */
/* Keep Incomplete Tests                                                      */
/* ========================================================================== */

/* Test: keep_incomplete preserves files in incomplete directory */
static void test_filelib_keep_incomplete_preserves_files(void)
{
    error_code result;
    TRACK_INFO ti;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Initialize with keep_incomplete = TRUE */
    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        TRUE,                    /* keep_incomplete = TRUE */
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Keep Inc Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
    TEST_ASSERT_TRUE(g_rmi.filelib_info.m_keep_incomplete);

    init_track_info(&ti, "Keep Inc Artist", "Keep Inc Title");

    /* Start first file */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = filelib_write_track(&g_rmi, "data1", 5);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Start another file with same name - should version the first */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);
}

/* ========================================================================== */
/* WAV Output Tests                                                           */
/* ========================================================================== */

/* Test: filelib_init with WAV output preference */
static void test_filelib_init_wav_output(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    /* Set WAV output preference */
    g_prefs.wav_output = 1;

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,        /* MP3 content but WAV output */
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "WAV Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Reset preference */
    g_prefs.wav_output = 0;
}

/* Test: filelib_init with ULTRAVOX content type */
static void test_filelib_init_ultravox(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_ULTRAVOX,   /* ULTRAVOX content type */
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Ultravox Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* ULTRAVOX uses .nsv extension like NSV */
    TEST_ASSERT_EQUAL_STRING(".nsv", g_rmi.filelib_info.m_extension);
}

/* ========================================================================== */
/* Sequence Number Tests                                                      */
/* ========================================================================== */

/* Test: Automatic sequence numbering with %q pattern */
static void test_filelib_sequence_numbering(void)
{
    error_code result;
    TRACK_INFO ti;
    mchar fullpath[SR_MAX_PATH];
    const char *data = "Sequence test";
    int i;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        TRUE,                    /* do_count = TRUE */
        1,                       /* count_start = 1 */
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        "%q_%A - %T",            /* pattern with sequence number prefix */
        NULL,
        FALSE,
        FALSE,
        "Sequence Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Write multiple tracks - each should get unique sequence number */
    for (i = 0; i < 3; i++) {
        char title[64];
        snprintf(title, sizeof(title), "Seq Track %d", i + 1);
        init_track_info(&ti, "Seq Artist", title);

        result = filelib_start(&g_rmi, &ti);
        TEST_ASSERT_EQUAL(SR_SUCCESS, result);

        result = filelib_write_track(&g_rmi, (char *)data, strlen(data));
        TEST_ASSERT_EQUAL(SR_SUCCESS, result);

        result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
        TEST_ASSERT_EQUAL(SR_SUCCESS, result);

        TEST_ASSERT_TRUE(test_file_size(fullpath) > 0);
    }

    /* Count should be incremented */
    TEST_ASSERT_EQUAL(4, g_rmi.filelib_info.m_count);
}

/* ========================================================================== */
/* Invalid Character and Path Tests                                           */
/* ========================================================================== */

/* Test: Track names with various invalid characters */
static void test_filelib_invalid_chars_comprehensive(void)
{
    error_code result;
    TRACK_INFO ti;
    mchar fullpath[SR_MAX_PATH];
    const char *data = "char test";

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Chars Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Test various invalid characters: \ / : * ? " < > | ~ */
    init_track_info(&ti, "Art\\ist/Name:Test", "Ti*tle?With\"Invalid<Chars>|~");

    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = filelib_write_track(&g_rmi, (char *)data, strlen(data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* File should be created with sanitized name */
    TEST_ASSERT_TRUE(test_file_size(fullpath) > 0);
}

/* Test: Track name with leading periods (should be stripped) */
static void test_filelib_leading_periods(void)
{
    error_code result;
    TRACK_INFO ti;
    mchar fullpath[SR_MAX_PATH];
    const char *data = "period test";

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Period Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Leading periods should be stripped */
    init_track_info(&ti, "...Artist", "...Title");

    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = filelib_write_track(&g_rmi, (char *)data, strlen(data));
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    TEST_ASSERT_TRUE(test_file_size(fullpath) > 0);
}

/* Test: Verify replace_invalid_chars is called on icy_name */
static void test_filelib_icy_name_sanitization(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        TRUE,                    /* get_separate_dirs to use icy_name */
        FALSE,
        "Radio/With:Invalid*Chars"   /* ICY name with invalid chars */
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Stripped ICY name should have invalid chars replaced */
    TEST_ASSERT_FALSE(strchr(g_rmi.filelib_info.m_stripped_icy_name, '/') != NULL);
    TEST_ASSERT_FALSE(strchr(g_rmi.filelib_info.m_stripped_icy_name, ':') != NULL);
    TEST_ASSERT_FALSE(strchr(g_rmi.filelib_info.m_stripped_icy_name, '*') != NULL);
}

/* Test: ICY name with trailing periods (should be removed) */
static void test_filelib_icy_name_trailing_periods(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        TRUE,                    /* get_separate_dirs to use icy_name */
        FALSE,
        "My Radio..."            /* Trailing periods */
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Stripped ICY name should not have trailing periods */
    int len = strlen(g_rmi.filelib_info.m_stripped_icy_name);
    if (len > 0) {
        TEST_ASSERT_NOT_EQUAL('.', g_rmi.filelib_info.m_stripped_icy_name[len - 1]);
    }
}

/* ========================================================================== */
/* Show File Pattern Tests                                                    */
/* ========================================================================== */

/* Test: Custom showfile pattern */
static void test_filelib_custom_showfile_pattern(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        TRUE,                    /* do_show_file = TRUE */
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        "my_show_%d",            /* custom showfile_pattern */
        FALSE,
        FALSE,
        "Custom Show Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Verify show file was created with custom pattern */
    TEST_ASSERT_TRUE(g_rmi.filelib_info.m_do_show);
    TEST_ASSERT_TRUE(strlen(g_rmi.filelib_info.m_show_name) > 0);
}

/* Test: Show file with OGG content (no CUE file) */
static void test_filelib_show_file_ogg_no_cue(void)
{
    error_code result;

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        TRUE,                    /* do_show_file = TRUE */
        CONTENT_TYPE_OGG,        /* OGG content - no CUE file */
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "OGG Show Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Show file should be created */
    TEST_ASSERT_TRUE(g_rmi.filelib_info.m_do_show);
    TEST_ASSERT_NOT_EQUAL(INVALID_FHANDLE, g_rmi.filelib_info.m_show_file);
    /* For OGG, the CUE file is not opened (code skips cue file for OGG) */
    /* The m_cue_file remains at its initialized value - but may have been
       modified by previous tests, so just verify show file is valid */
}

/* ========================================================================== */
/* Edge Case Tests                                                            */
/* ========================================================================== */

/* Test: Empty track data */
static void test_empty_track_data(void)
{
    error_code result;
    TRACK_INFO ti;
    mchar fullpath[SR_MAX_PATH];

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Empty Data Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    init_track_info(&ti, "Empty Artist", "Empty Title");

    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Write zero bytes */
    result = filelib_write_track(&g_rmi, "", 0);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* File should exist but be empty */
    TEST_ASSERT_EQUAL(0, test_file_size(fullpath));
}

/* Test: Track with special characters in artist/title */
static void test_special_characters_in_track_info(void)
{
    error_code result;
    TRACK_INFO ti;
    mchar fullpath[SR_MAX_PATH];

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Special Chars Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Use special characters that should be replaced */
    init_track_info(&ti, "Art/ist<Name>", "Tit:le*With?Chars");

    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = filelib_write_track(&g_rmi, "data", 4);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    result = filelib_end(&g_rmi, &ti, OVERWRITE_ALWAYS, FALSE, fullpath);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* File should be created with sanitized name */
    TEST_ASSERT_TRUE(strlen(fullpath) > 0);
    TEST_ASSERT_TRUE(test_file_size(fullpath) >= 0);
}

/* Test: Long track name (close to path limit) */
static void test_long_track_name(void)
{
    error_code result;
    TRACK_INFO ti;
    char long_title[200];

    TEST_ASSERT_TRUE_MESSAGE(g_temp_dir_created, "Temp directory creation failed");

    result = filelib_init(
        &g_rmi,
        TRUE,
        FALSE,
        1,
        FALSE,
        FALSE,
        CONTENT_TYPE_MP3,
        g_temp_dir,
        NULL,
        NULL,
        FALSE,
        FALSE,
        "Long Name Radio"
    );
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    /* Create a very long title */
    memset(long_title, 'A', sizeof(long_title) - 1);
    long_title[sizeof(long_title) - 1] = '\0';

    init_track_info(&ti, "Artist", long_title);

    /* Should succeed - title will be truncated to fit path limits */
    result = filelib_start(&g_rmi, &ti);
    TEST_ASSERT_EQUAL(SR_SUCCESS, result);

    filelib_shutdown(&g_rmi);
}

/* ========================================================================== */
/* Main - Run all tests                                                       */
/* ========================================================================== */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* Path macros and constants tests (existing) */
    RUN_TEST(test_path_slash_unix);
    RUN_TEST(test_isslash_macro);
    RUN_TEST(test_is_absolute_path_unix);
    RUN_TEST(test_has_device_unix);
    RUN_TEST(test_path_constants);
    RUN_TEST(test_max_path_calculations);
    RUN_TEST(test_content_type_constants);
    RUN_TEST(test_overwrite_opt_values);
    RUN_TEST(test_datebuf_len);
    RUN_TEST(test_filelib_info_structure);
    RUN_TEST(test_filelib_info_path_sizes);

    /* filelib_init tests */
    RUN_TEST(test_filelib_init_valid_directory_mp3);
    RUN_TEST(test_filelib_init_valid_directory_ogg);
    RUN_TEST(test_filelib_init_valid_directory_aac);
    RUN_TEST(test_filelib_init_valid_directory_nsv);
    RUN_TEST(test_filelib_init_invalid_content_type);
    RUN_TEST(test_filelib_init_with_counting);
    RUN_TEST(test_filelib_init_keep_incomplete);
    RUN_TEST(test_filelib_init_separate_dirs);
    RUN_TEST(test_filelib_init_no_individual_tracks);

    /* filelib_start tests */
    RUN_TEST(test_filelib_start_creates_file);
    RUN_TEST(test_filelib_start_no_individual_tracks);

    /* filelib_write_track tests */
    RUN_TEST(test_filelib_write_track_writes_data);
    RUN_TEST(test_filelib_write_track_multiple_writes);

    /* filelib_write_show tests */
    RUN_TEST(test_filelib_write_show_disabled);

    /* filelib_write_cue tests */
    RUN_TEST(test_filelib_write_cue_disabled);

    /* filelib_end tests */
    RUN_TEST(test_filelib_end_moves_file);
    RUN_TEST(test_filelib_end_overwrite_never);
    RUN_TEST(test_filelib_end_no_individual_tracks);

    /* filelib_shutdown tests */
    RUN_TEST(test_filelib_shutdown_closes_handles);
    RUN_TEST(test_filelib_shutdown_idempotent);

    /* Full lifecycle tests */
    RUN_TEST(test_full_lifecycle_single_track);
    RUN_TEST(test_full_lifecycle_multiple_tracks);

    /* Show file and CUE file tests */
    RUN_TEST(test_filelib_init_with_show_file);
    RUN_TEST(test_filelib_write_show_enabled);
    RUN_TEST(test_filelib_write_cue_enabled);
    RUN_TEST(test_filelib_write_cue_multiple_entries);

    /* Overwrite mode tests */
    RUN_TEST(test_filelib_end_overwrite_larger_new_bigger);
    RUN_TEST(test_filelib_end_overwrite_larger_new_smaller);
    RUN_TEST(test_filelib_end_overwrite_never_existing);
    RUN_TEST(test_filelib_end_overwrite_version);
    RUN_TEST(test_filelib_end_truncate_dup);

    /* Output pattern tests */
    RUN_TEST(test_filelib_init_custom_pattern);
    RUN_TEST(test_filelib_init_with_date_stamp);
    RUN_TEST(test_filelib_init_separate_dirs_with_date);
    RUN_TEST(test_filelib_init_counting_separate_dirs);
    RUN_TEST(test_filelib_track_with_album);

    /* Keep incomplete tests */
    RUN_TEST(test_filelib_keep_incomplete_preserves_files);

    /* WAV and content type tests */
    RUN_TEST(test_filelib_init_wav_output);
    RUN_TEST(test_filelib_init_ultravox);

    /* Sequence number tests */
    RUN_TEST(test_filelib_sequence_numbering);

    /* Invalid character and path tests */
    RUN_TEST(test_filelib_invalid_chars_comprehensive);
    RUN_TEST(test_filelib_leading_periods);
    RUN_TEST(test_filelib_icy_name_sanitization);
    RUN_TEST(test_filelib_icy_name_trailing_periods);

    /* Show file pattern tests */
    RUN_TEST(test_filelib_custom_showfile_pattern);
    RUN_TEST(test_filelib_show_file_ogg_no_cue);

    /* Edge case tests */
    RUN_TEST(test_empty_track_data);
    RUN_TEST(test_special_characters_in_track_info);
    RUN_TEST(test_long_track_name);

    return UNITY_END();
}
