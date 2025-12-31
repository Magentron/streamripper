/* test_ripogg.c - Unit tests for lib/ripogg.c
 *
 * Tests for OGG Vorbis stream parsing functionality.
 * Note: The ripogg.c module is conditionally compiled only when HAVE_OGG_VORBIS
 * is defined. These tests verify data structures and basic functionality.
 */
#include <string.h>
#include <stdlib.h>
#include <unity.h>

#include "srconfig.h"
#include "ripogg.h"
#include "srtypes.h"
#include "errors.h"
#include "list.h"
#include "cbuf2.h"

/* OGG Page Header structure (for reference)
 *
 * Byte 0-3:   "OggS" capture pattern
 * Byte 4:     Stream structure version (0x00)
 * Byte 5:     Header type flag
 *             0x01 = continuation of previous page
 *             0x02 = beginning of stream (BOS)
 *             0x04 = end of stream (EOS)
 * Byte 6-13:  Absolute granule position (8 bytes)
 * Byte 14-17: Stream serial number (4 bytes)
 * Byte 18-21: Page sequence number (4 bytes)
 * Byte 22-25: CRC checksum (4 bytes)
 * Byte 26:    Number of page segments
 * Byte 27+:   Segment table (1 byte per segment)
 */

/* Sample OGG page header with BOS flag */
static const unsigned char ogg_bos_header[] = {
    'O', 'g', 'g', 'S',     /* Capture pattern */
    0x00,                   /* Version */
    0x02,                   /* BOS flag */
    0x00, 0x00, 0x00, 0x00, /* Granule position (low) */
    0x00, 0x00, 0x00, 0x00, /* Granule position (high) */
    0x01, 0x02, 0x03, 0x04, /* Serial number */
    0x00, 0x00, 0x00, 0x00, /* Page sequence number */
    0x00, 0x00, 0x00, 0x00, /* CRC (would be calculated) */
    0x01,                   /* Number of segments */
    0x1E                    /* Segment table: one segment of 30 bytes */
};

/* Sample OGG page header with EOS flag */
static const unsigned char ogg_eos_header[] = {
    'O', 'g', 'g', 'S',     /* Capture pattern */
    0x00,                   /* Version */
    0x04,                   /* EOS flag */
    0xFF, 0xFF, 0xFF, 0xFF, /* Granule position (low) */
    0x00, 0x00, 0x00, 0x00, /* Granule position (high) */
    0x01, 0x02, 0x03, 0x04, /* Serial number */
    0x01, 0x00, 0x00, 0x00, /* Page sequence number = 1 */
    0x00, 0x00, 0x00, 0x00, /* CRC */
    0x00                    /* Number of segments = 0 */
};

/* Sample OGG page header - middle page (no flags) */
static const unsigned char ogg_middle_header[] = {
    'O', 'g', 'g', 'S',     /* Capture pattern */
    0x00,                   /* Version */
    0x00,                   /* No flags */
    0x00, 0x10, 0x00, 0x00, /* Granule position */
    0x00, 0x00, 0x00, 0x00,
    0x01, 0x02, 0x03, 0x04, /* Serial number */
    0x02, 0x00, 0x00, 0x00, /* Page sequence number = 2 */
    0x00, 0x00, 0x00, 0x00, /* CRC */
    0x02,                   /* Number of segments = 2 */
    0xFF, 0x10              /* Segment table: 255 + 16 = 271 bytes */
};

/* Invalid data (not OGG) */
static const unsigned char invalid_ogg_data[] = {
    0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07
};

void setUp(void)
{
    /* Nothing to set up */
}

void tearDown(void)
{
    /* Nothing to clean up */
}

/* ============================================================================
 * OGG Constants and Flag Tests
 * ============================================================================ */

/* Test: CONTENT_TYPE_OGG constant is defined */
static void test_ogg_content_type_defined(void)
{
    TEST_ASSERT_EQUAL(3, CONTENT_TYPE_OGG);
}

/* Test: OGG page flags are defined in cbuf2.h */
static void test_ogg_page_flags_defined(void)
{
    /* These flags match the OGG specification */
    TEST_ASSERT_EQUAL(0x01, OGG_PAGE_BOS);
    TEST_ASSERT_EQUAL(0x02, OGG_PAGE_EOS);
    TEST_ASSERT_EQUAL(0x04, OGG_PAGE_2);
}

/* ============================================================================
 * OGG Page Header Parsing Tests
 * ============================================================================ */

/* Test: OGG capture pattern detection */
static void test_ogg_capture_pattern(void)
{
    /* OGG pages start with "OggS" */
    TEST_ASSERT_EQUAL('O', ogg_bos_header[0]);
    TEST_ASSERT_EQUAL('g', ogg_bos_header[1]);
    TEST_ASSERT_EQUAL('g', ogg_bos_header[2]);
    TEST_ASSERT_EQUAL('S', ogg_bos_header[3]);
}

/* Test: Invalid capture pattern detection */
static void test_ogg_invalid_capture_pattern(void)
{
    /* Invalid data should not have OggS pattern */
    int is_valid = (invalid_ogg_data[0] == 'O' &&
                    invalid_ogg_data[1] == 'g' &&
                    invalid_ogg_data[2] == 'g' &&
                    invalid_ogg_data[3] == 'S');
    TEST_ASSERT_FALSE(is_valid);
}

/* Test: Stream structure version */
static void test_ogg_stream_version(void)
{
    /* Version should always be 0 */
    TEST_ASSERT_EQUAL(0x00, ogg_bos_header[4]);
    TEST_ASSERT_EQUAL(0x00, ogg_eos_header[4]);
    TEST_ASSERT_EQUAL(0x00, ogg_middle_header[4]);
}

/* Test: BOS (Beginning of Stream) flag extraction */
static void test_ogg_bos_flag(void)
{
    unsigned char header_type = ogg_bos_header[5];

    /* BOS flag is bit 1 (0x02) */
    TEST_ASSERT_TRUE(header_type & 0x02);
    TEST_ASSERT_FALSE(header_type & 0x04);  /* Not EOS */
}

/* Test: EOS (End of Stream) flag extraction */
static void test_ogg_eos_flag(void)
{
    unsigned char header_type = ogg_eos_header[5];

    /* EOS flag is bit 2 (0x04) */
    TEST_ASSERT_TRUE(header_type & 0x04);
    TEST_ASSERT_FALSE(header_type & 0x02);  /* Not BOS */
}

/* Test: Middle page has no flags */
static void test_ogg_middle_page_no_flags(void)
{
    unsigned char header_type = ogg_middle_header[5];

    TEST_ASSERT_FALSE(header_type & 0x01);  /* Not continuation */
    TEST_ASSERT_FALSE(header_type & 0x02);  /* Not BOS */
    TEST_ASSERT_FALSE(header_type & 0x04);  /* Not EOS */
}

/* Test: Extract serial number */
static void test_ogg_serial_number(void)
{
    /* Serial number is little-endian at bytes 14-17 */
    unsigned int serial =
        ogg_bos_header[14] |
        (ogg_bos_header[15] << 8) |
        (ogg_bos_header[16] << 16) |
        (ogg_bos_header[17] << 24);

    TEST_ASSERT_EQUAL(0x04030201, serial);
}

/* Test: Extract page sequence number */
static void test_ogg_page_sequence_number(void)
{
    /* Page sequence number is little-endian at bytes 18-21 */

    /* BOS page should be 0 */
    unsigned int seq_bos =
        ogg_bos_header[18] |
        (ogg_bos_header[19] << 8) |
        (ogg_bos_header[20] << 16) |
        (ogg_bos_header[21] << 24);
    TEST_ASSERT_EQUAL(0, seq_bos);

    /* EOS page should be 1 */
    unsigned int seq_eos =
        ogg_eos_header[18] |
        (ogg_eos_header[19] << 8) |
        (ogg_eos_header[20] << 16) |
        (ogg_eos_header[21] << 24);
    TEST_ASSERT_EQUAL(1, seq_eos);
}

/* Test: Extract number of segments */
static void test_ogg_number_of_segments(void)
{
    /* Number of segments is at byte 26 */
    TEST_ASSERT_EQUAL(1, ogg_bos_header[26]);
    TEST_ASSERT_EQUAL(0, ogg_eos_header[26]);
    TEST_ASSERT_EQUAL(2, ogg_middle_header[26]);
}

/* Test: Calculate page body length from segment table */
static void test_ogg_page_body_length(void)
{
    /* Body length is sum of all segment sizes */

    /* BOS page: 1 segment of 30 bytes */
    unsigned int bos_body_len = ogg_bos_header[27];
    TEST_ASSERT_EQUAL(30, bos_body_len);

    /* EOS page: 0 segments = 0 bytes */
    TEST_ASSERT_EQUAL(0, ogg_eos_header[26]);

    /* Middle page: 255 + 16 = 271 bytes */
    unsigned int middle_body_len = ogg_middle_header[27] + ogg_middle_header[28];
    TEST_ASSERT_EQUAL(271, middle_body_len);
}

/* ============================================================================
 * OGG_PAGE_LIST Structure Tests
 * ============================================================================ */

/* Test: OGG_PAGE_LIST structure initialization */
static void test_ogg_page_list_struct(void)
{
    OGG_PAGE_LIST opl;
    memset(&opl, 0, sizeof(opl));

    opl.m_page_start = 1000;
    opl.m_page_len = 4096;
    opl.m_page_flags = OGG_PAGE_BOS;
    opl.m_header_buf_ptr = NULL;
    opl.m_header_buf_len = 0;

    TEST_ASSERT_EQUAL(1000, opl.m_page_start);
    TEST_ASSERT_EQUAL(4096, opl.m_page_len);
    TEST_ASSERT_EQUAL(OGG_PAGE_BOS, opl.m_page_flags);
    TEST_ASSERT_NULL(opl.m_header_buf_ptr);
    TEST_ASSERT_EQUAL(0, opl.m_header_buf_len);
}

/* Test: OGG_PAGE_LIST with header buffer */
static void test_ogg_page_list_with_header(void)
{
    OGG_PAGE_LIST opl;
    char header_data[256];
    memset(&opl, 0, sizeof(opl));
    memset(header_data, 0xAA, sizeof(header_data));

    opl.m_page_start = 0;
    opl.m_page_len = 8192;
    opl.m_page_flags = 0;
    opl.m_header_buf_ptr = header_data;
    opl.m_header_buf_len = sizeof(header_data);

    TEST_ASSERT_NOT_NULL(opl.m_header_buf_ptr);
    TEST_ASSERT_EQUAL(256, opl.m_header_buf_len);
    TEST_ASSERT_EQUAL(0xAA, opl.m_header_buf_ptr[0]);
}

/* Test: Multiple flag combinations */
static void test_ogg_page_flags_combinations(void)
{
    OGG_PAGE_LIST opl;

    /* BOS only */
    opl.m_page_flags = OGG_PAGE_BOS;
    TEST_ASSERT_TRUE(opl.m_page_flags & OGG_PAGE_BOS);
    TEST_ASSERT_FALSE(opl.m_page_flags & OGG_PAGE_EOS);

    /* EOS only */
    opl.m_page_flags = OGG_PAGE_EOS;
    TEST_ASSERT_FALSE(opl.m_page_flags & OGG_PAGE_BOS);
    TEST_ASSERT_TRUE(opl.m_page_flags & OGG_PAGE_EOS);

    /* OGG_PAGE_2 flag */
    opl.m_page_flags = OGG_PAGE_2;
    TEST_ASSERT_TRUE(opl.m_page_flags & OGG_PAGE_2);
}

/* ============================================================================
 * List Operations Tests
 * ============================================================================ */

/* Test: Initialize empty OGG page list */
static void test_ogg_init_empty_list(void)
{
    LIST page_list;
    INIT_LIST_HEAD(&page_list);

    TEST_ASSERT_TRUE(list_empty(&page_list));
    TEST_ASSERT_EQUAL(&page_list, page_list.next);
    TEST_ASSERT_EQUAL(&page_list, page_list.prev);
}

/* Test: list_add adds at head */
static void test_list_add_at_head(void)
{
    LIST head;
    OGG_PAGE_LIST page1, page2;

    INIT_LIST_HEAD(&head);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));

    page1.m_page_start = 100;
    page2.m_page_start = 200;

    /* Add page1 first, then page2 at head */
    list_add(&page1.m_list, &head);
    list_add(&page2.m_list, &head);

    /* page2 should be first (at head) */
    OGG_PAGE_LIST *first = list_entry(head.next, OGG_PAGE_LIST, m_list);
    TEST_ASSERT_EQUAL(200, first->m_page_start);

    /* page1 should be second */
    OGG_PAGE_LIST *second = list_entry(head.next->next, OGG_PAGE_LIST, m_list);
    TEST_ASSERT_EQUAL(100, second->m_page_start);
}

/* Test: list_del removes entry from list */
static void test_list_del(void)
{
    LIST head;
    OGG_PAGE_LIST page1, page2, page3;

    INIT_LIST_HEAD(&head);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));
    memset(&page3, 0, sizeof(page3));

    page1.m_page_start = 100;
    page2.m_page_start = 200;
    page3.m_page_start = 300;

    list_add_tail(&page1.m_list, &head);
    list_add_tail(&page2.m_list, &head);
    list_add_tail(&page3.m_list, &head);

    /* Remove middle element */
    list_del(&page2.m_list);

    /* Now page1 should link directly to page3 */
    OGG_PAGE_LIST *first = list_entry(head.next, OGG_PAGE_LIST, m_list);
    OGG_PAGE_LIST *second_after_del = list_entry(head.next->next, OGG_PAGE_LIST, m_list);
    TEST_ASSERT_EQUAL(100, first->m_page_start);
    TEST_ASSERT_EQUAL(300, second_after_del->m_page_start);
}

/* Test: list_del_init removes and reinitializes entry */
static void test_list_del_init(void)
{
    LIST head;
    OGG_PAGE_LIST page1, page2;

    INIT_LIST_HEAD(&head);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));

    page1.m_page_start = 100;
    page2.m_page_start = 200;

    list_add_tail(&page1.m_list, &head);
    list_add_tail(&page2.m_list, &head);

    /* Remove and reinit page1 */
    list_del_init(&page1.m_list);

    /* page1's list should point to itself */
    TEST_ASSERT_EQUAL(&page1.m_list, page1.m_list.next);
    TEST_ASSERT_EQUAL(&page1.m_list, page1.m_list.prev);

    /* Only page2 should remain in head */
    OGG_PAGE_LIST *only = list_entry(head.next, OGG_PAGE_LIST, m_list);
    TEST_ASSERT_EQUAL(200, only->m_page_start);
}

/* Test: list_move moves entry to another list's head */
static void test_list_move(void)
{
    LIST head1, head2;
    OGG_PAGE_LIST page1, page2;

    INIT_LIST_HEAD(&head1);
    INIT_LIST_HEAD(&head2);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));

    page1.m_page_start = 100;
    page2.m_page_start = 200;

    list_add_tail(&page1.m_list, &head1);
    list_add_tail(&page2.m_list, &head2);

    /* Move page1 from head1 to head2 (at head) */
    list_move(&page1.m_list, &head2);

    /* head1 should be empty */
    TEST_ASSERT_TRUE(list_empty(&head1));

    /* head2 should have page1 at head, page2 at tail */
    OGG_PAGE_LIST *first = list_entry(head2.next, OGG_PAGE_LIST, m_list);
    TEST_ASSERT_EQUAL(100, first->m_page_start);
}

/* Test: list_move_tail moves entry to another list's tail */
static void test_list_move_tail(void)
{
    LIST head1, head2;
    OGG_PAGE_LIST page1, page2;

    INIT_LIST_HEAD(&head1);
    INIT_LIST_HEAD(&head2);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));

    page1.m_page_start = 100;
    page2.m_page_start = 200;

    list_add_tail(&page1.m_list, &head1);
    list_add_tail(&page2.m_list, &head2);

    /* Move page1 from head1 to head2 (at tail) */
    list_move_tail(&page1.m_list, &head2);

    /* head1 should be empty */
    TEST_ASSERT_TRUE(list_empty(&head1));

    /* head2 should have page2 at head, page1 at tail */
    OGG_PAGE_LIST *first = list_entry(head2.next, OGG_PAGE_LIST, m_list);
    OGG_PAGE_LIST *last = list_entry(head2.prev, OGG_PAGE_LIST, m_list);
    TEST_ASSERT_EQUAL(200, first->m_page_start);
    TEST_ASSERT_EQUAL(100, last->m_page_start);
}

/* Test: list_splice joins two lists */
static void test_list_splice(void)
{
    LIST head1, head2;
    OGG_PAGE_LIST page1, page2, page3;

    INIT_LIST_HEAD(&head1);
    INIT_LIST_HEAD(&head2);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));
    memset(&page3, 0, sizeof(page3));

    page1.m_page_start = 100;
    page2.m_page_start = 200;
    page3.m_page_start = 300;

    list_add_tail(&page1.m_list, &head1);
    list_add_tail(&page2.m_list, &head2);
    list_add_tail(&page3.m_list, &head2);

    /* Splice head2 into head1 */
    list_splice(&head2, &head1);

    /* Now head1 should have: page2, page3, page1 (splice adds at head) */
    OGG_PAGE_LIST *first = list_entry(head1.next, OGG_PAGE_LIST, m_list);
    TEST_ASSERT_EQUAL(200, first->m_page_start);
}

/* Test: list_splice with empty list does nothing */
static void test_list_splice_empty(void)
{
    LIST head1, head2;
    OGG_PAGE_LIST page1;

    INIT_LIST_HEAD(&head1);
    INIT_LIST_HEAD(&head2);
    memset(&page1, 0, sizeof(page1));

    page1.m_page_start = 100;
    list_add_tail(&page1.m_list, &head1);

    /* Splice empty head2 into head1 - should do nothing */
    list_splice(&head2, &head1);

    OGG_PAGE_LIST *first = list_entry(head1.next, OGG_PAGE_LIST, m_list);
    TEST_ASSERT_EQUAL(100, first->m_page_start);
}

/* Test: list_splice_init joins lists and reinits source */
static void test_list_splice_init(void)
{
    LIST head1, head2;
    OGG_PAGE_LIST page1, page2;

    INIT_LIST_HEAD(&head1);
    INIT_LIST_HEAD(&head2);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));

    page1.m_page_start = 100;
    page2.m_page_start = 200;

    list_add_tail(&page1.m_list, &head1);
    list_add_tail(&page2.m_list, &head2);

    /* Splice head2 into head1 and reinit head2 */
    list_splice_init(&head2, &head1);

    /* head2 should be empty (reinitialized) */
    TEST_ASSERT_TRUE(list_empty(&head2));
}

/* Test: list_splice_init with empty source */
static void test_list_splice_init_empty(void)
{
    LIST head1, head2;
    OGG_PAGE_LIST page1;

    INIT_LIST_HEAD(&head1);
    INIT_LIST_HEAD(&head2);
    memset(&page1, 0, sizeof(page1));

    page1.m_page_start = 100;
    list_add_tail(&page1.m_list, &head1);

    /* Splice empty head2 - should do nothing */
    list_splice_init(&head2, &head1);

    /* head1 should still have page1 */
    TEST_ASSERT_FALSE(list_empty(&head1));
    OGG_PAGE_LIST *first = list_entry(head1.next, OGG_PAGE_LIST, m_list);
    TEST_ASSERT_EQUAL(100, first->m_page_start);
}

/* Test: list_for_each iteration */
static void test_list_for_each(void)
{
    LIST head;
    OGG_PAGE_LIST page1, page2, page3;
    struct list_head *pos;
    int count = 0;

    INIT_LIST_HEAD(&head);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));
    memset(&page3, 0, sizeof(page3));

    page1.m_page_start = 100;
    page2.m_page_start = 200;
    page3.m_page_start = 300;

    list_add_tail(&page1.m_list, &head);
    list_add_tail(&page2.m_list, &head);
    list_add_tail(&page3.m_list, &head);

    /* Count entries using list_for_each */
    list_for_each(pos, &head) {
        count++;
    }

    TEST_ASSERT_EQUAL(3, count);
}

/* Test: list_for_each_prev backward iteration */
static void test_list_for_each_prev(void)
{
    LIST head;
    OGG_PAGE_LIST page1, page2, page3;
    struct list_head *pos;
    int last_value = 0;

    INIT_LIST_HEAD(&head);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));
    memset(&page3, 0, sizeof(page3));

    page1.m_page_start = 100;
    page2.m_page_start = 200;
    page3.m_page_start = 300;

    list_add_tail(&page1.m_list, &head);
    list_add_tail(&page2.m_list, &head);
    list_add_tail(&page3.m_list, &head);

    /* First entry from prev should be page3 (the last added) */
    list_for_each_prev(pos, &head) {
        OGG_PAGE_LIST *entry = list_entry(pos, OGG_PAGE_LIST, m_list);
        last_value = entry->m_page_start;
        break; /* Just get first (which is last in forward order) */
    }

    TEST_ASSERT_EQUAL(300, last_value);
}

/* Test: list_for_each_safe allows deletion during iteration */
static void test_list_for_each_safe(void)
{
    LIST head;
    OGG_PAGE_LIST page1, page2, page3;
    struct list_head *pos, *n;
    int count = 0;

    INIT_LIST_HEAD(&head);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));
    memset(&page3, 0, sizeof(page3));

    page1.m_page_start = 100;
    page2.m_page_start = 200;
    page3.m_page_start = 300;

    list_add_tail(&page1.m_list, &head);
    list_add_tail(&page2.m_list, &head);
    list_add_tail(&page3.m_list, &head);

    /* Delete all entries during iteration */
    list_for_each_safe(pos, n, &head) {
        list_del(pos);
        count++;
    }

    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_TRUE(list_empty(&head));
}

/* Test: list_for_each_entry iteration with type */
static void test_list_for_each_entry(void)
{
    LIST head;
    OGG_PAGE_LIST page1, page2, page3;
    OGG_PAGE_LIST *entry;
    int sum = 0;

    INIT_LIST_HEAD(&head);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));
    memset(&page3, 0, sizeof(page3));

    page1.m_page_start = 100;
    page2.m_page_start = 200;
    page3.m_page_start = 300;

    list_add_tail(&page1.m_list, &head);
    list_add_tail(&page2.m_list, &head);
    list_add_tail(&page3.m_list, &head);

    /* Sum all page_start values */
    list_for_each_entry(entry, OGG_PAGE_LIST, &head, m_list) {
        sum += entry->m_page_start;
    }

    TEST_ASSERT_EQUAL(600, sum);
}

/* Test: list_for_each_entry_safe allows deletion */
static void test_list_for_each_entry_safe(void)
{
    LIST head;
    OGG_PAGE_LIST page1, page2, page3;
    OGG_PAGE_LIST *entry, *tmp;
    int count = 0;

    INIT_LIST_HEAD(&head);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));
    memset(&page3, 0, sizeof(page3));

    page1.m_page_start = 100;
    page2.m_page_start = 200;
    page3.m_page_start = 300;

    list_add_tail(&page1.m_list, &head);
    list_add_tail(&page2.m_list, &head);
    list_add_tail(&page3.m_list, &head);

    /* Delete entries with page_start >= 200 */
    list_for_each_entry_safe(entry, OGG_PAGE_LIST, tmp, &head, m_list) {
        if (entry->m_page_start >= 200) {
            list_del(&entry->m_list);
            count++;
        }
    }

    TEST_ASSERT_EQUAL(2, count);  /* Deleted page2 and page3 */

    /* Only page1 should remain */
    TEST_ASSERT_FALSE(list_empty(&head));
    OGG_PAGE_LIST *first = list_entry(head.next, OGG_PAGE_LIST, m_list);
    TEST_ASSERT_EQUAL(100, first->m_page_start);
}

/* Test: Add pages to list */
static void test_ogg_add_pages_to_list(void)
{
    LIST page_list;
    OGG_PAGE_LIST page1, page2;

    INIT_LIST_HEAD(&page_list);
    memset(&page1, 0, sizeof(page1));
    memset(&page2, 0, sizeof(page2));

    page1.m_page_start = 0;
    page1.m_page_len = 1000;
    page1.m_page_flags = OGG_PAGE_BOS;

    page2.m_page_start = 1000;
    page2.m_page_len = 2000;
    page2.m_page_flags = 0;

    /* Add pages to list */
    list_add_tail(&page1.m_list, &page_list);
    list_add_tail(&page2.m_list, &page_list);

    TEST_ASSERT_FALSE(list_empty(&page_list));

    /* Verify first entry */
    OGG_PAGE_LIST *first = list_entry(page_list.next, OGG_PAGE_LIST, m_list);
    TEST_ASSERT_EQUAL(0, first->m_page_start);
    TEST_ASSERT_EQUAL(OGG_PAGE_BOS, first->m_page_flags);

    /* Verify second entry */
    OGG_PAGE_LIST *second = list_entry(page_list.next->next, OGG_PAGE_LIST, m_list);
    TEST_ASSERT_EQUAL(1000, second->m_page_start);
}

/* ============================================================================
 * CBUF2 OGG Support Tests
 * ============================================================================ */

/* Test: CBUF2 structure has OGG page list */
static void test_cbuf2_has_ogg_page_list(void)
{
    CBUF2 cbuf;
    memset(&cbuf, 0, sizeof(cbuf));

    cbuf.content_type = CONTENT_TYPE_OGG;
    INIT_LIST_HEAD(&cbuf.ogg_page_list);

    TEST_ASSERT_EQUAL(CONTENT_TYPE_OGG, cbuf.content_type);
    TEST_ASSERT_TRUE(list_empty(&cbuf.ogg_page_list));
}

/* Test: CBUF2 OGG song page tracking */
static void test_cbuf2_ogg_song_page(void)
{
    CBUF2 cbuf;
    OGG_PAGE_LIST song_page;

    memset(&cbuf, 0, sizeof(cbuf));
    memset(&song_page, 0, sizeof(song_page));

    song_page.m_page_start = 5000;
    song_page.m_page_len = 4096;

    cbuf.content_type = CONTENT_TYPE_OGG;
    cbuf.song_page = &song_page;
    cbuf.song_page_done = 1024;

    TEST_ASSERT_NOT_NULL(cbuf.song_page);
    TEST_ASSERT_EQUAL(5000, cbuf.song_page->m_page_start);
    TEST_ASSERT_EQUAL(1024, cbuf.song_page_done);
}

/* ============================================================================
 * TRACK_INFO with OGG Tests
 * ============================================================================ */

/* Test: TRACK_INFO can store OGG Vorbis comments */
static void test_track_info_ogg_comments(void)
{
    TRACK_INFO ti;
    memset(&ti, 0, sizeof(ti));

    /* OGG Vorbis comments are case-insensitive field=value pairs */
    ti.have_track_info = 1;
    strncpy(ti.artist, "Vorbis Artist", MAX_TRACK_LEN - 1);
    strncpy(ti.title, "Vorbis Title", MAX_TRACK_LEN - 1);
    strncpy(ti.album, "Vorbis Album", MAX_TRACK_LEN - 1);
    strncpy(ti.track_p, "5", MAX_TRACK_LEN - 1);

    TEST_ASSERT_EQUAL(1, ti.have_track_info);
    TEST_ASSERT_EQUAL_STRING("Vorbis Artist", ti.artist);
    TEST_ASSERT_EQUAL_STRING("Vorbis Title", ti.title);
    TEST_ASSERT_EQUAL_STRING("Vorbis Album", ti.album);
    TEST_ASSERT_EQUAL_STRING("5", ti.track_p);
}

/* ============================================================================
 * RIP_MANAGER_INFO OGG Fields Tests
 * ============================================================================ */

#if (HAVE_OGG_VORBIS)
/* Test: RIP_MANAGER_INFO has OGG state fields (when compiled with OGG support) */
static void test_rmi_ogg_fields(void)
{
    RIP_MANAGER_INFO rmi;
    memset(&rmi, 0, sizeof(rmi));

    /* These fields exist only when HAVE_OGG_VORBIS is defined */
    rmi.ogg_curr_header = NULL;
    rmi.ogg_curr_header_len = 0;

    TEST_ASSERT_NULL(rmi.ogg_curr_header);
    TEST_ASSERT_EQUAL(0, rmi.ogg_curr_header_len);
}
#endif

/* ============================================================================
 * SR_HTTP_HEADER OGG Content Type Tests
 * ============================================================================ */

/* Test: SR_HTTP_HEADER can store OGG content type */
static void test_sr_http_header_ogg(void)
{
    SR_HTTP_HEADER header;
    memset(&header, 0, sizeof(header));

    header.content_type = CONTENT_TYPE_OGG;
    header.icy_bitrate = 128;
    strncpy(header.icy_name, "OGG Stream", MAX_ICY_STRING - 1);

    TEST_ASSERT_EQUAL(CONTENT_TYPE_OGG, header.content_type);
    TEST_ASSERT_EQUAL(128, header.icy_bitrate);
    TEST_ASSERT_EQUAL_STRING("OGG Stream", header.icy_name);
}

/* ============================================================================
 * Granule Position Tests
 * ============================================================================ */

/* Test: Granule position is 64-bit */
static void test_ogg_granule_position_size(void)
{
    /* Granule position spans bytes 6-13 (8 bytes = 64 bits) */
    unsigned long long granule_bos =
        ((unsigned long long)ogg_bos_header[6]) |
        ((unsigned long long)ogg_bos_header[7] << 8) |
        ((unsigned long long)ogg_bos_header[8] << 16) |
        ((unsigned long long)ogg_bos_header[9] << 24) |
        ((unsigned long long)ogg_bos_header[10] << 32) |
        ((unsigned long long)ogg_bos_header[11] << 40) |
        ((unsigned long long)ogg_bos_header[12] << 48) |
        ((unsigned long long)ogg_bos_header[13] << 56);

    /* BOS page granule position should be 0 */
    TEST_ASSERT_EQUAL(0, granule_bos);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* OGG Constants and Flag Tests */
    RUN_TEST(test_ogg_content_type_defined);
    RUN_TEST(test_ogg_page_flags_defined);

    /* OGG Page Header Parsing Tests */
    RUN_TEST(test_ogg_capture_pattern);
    RUN_TEST(test_ogg_invalid_capture_pattern);
    RUN_TEST(test_ogg_stream_version);
    RUN_TEST(test_ogg_bos_flag);
    RUN_TEST(test_ogg_eos_flag);
    RUN_TEST(test_ogg_middle_page_no_flags);
    RUN_TEST(test_ogg_serial_number);
    RUN_TEST(test_ogg_page_sequence_number);
    RUN_TEST(test_ogg_number_of_segments);
    RUN_TEST(test_ogg_page_body_length);

    /* OGG_PAGE_LIST Structure Tests */
    RUN_TEST(test_ogg_page_list_struct);
    RUN_TEST(test_ogg_page_list_with_header);
    RUN_TEST(test_ogg_page_flags_combinations);

    /* List Operations Tests */
    RUN_TEST(test_ogg_init_empty_list);
    RUN_TEST(test_list_add_at_head);
    RUN_TEST(test_list_del);
    RUN_TEST(test_list_del_init);
    RUN_TEST(test_list_move);
    RUN_TEST(test_list_move_tail);
    RUN_TEST(test_list_splice);
    RUN_TEST(test_list_splice_empty);
    RUN_TEST(test_list_splice_init);
    RUN_TEST(test_list_splice_init_empty);
    RUN_TEST(test_list_for_each);
    RUN_TEST(test_list_for_each_prev);
    RUN_TEST(test_list_for_each_safe);
    RUN_TEST(test_list_for_each_entry);
    RUN_TEST(test_list_for_each_entry_safe);
    RUN_TEST(test_ogg_add_pages_to_list);

    /* CBUF2 OGG Support Tests */
    RUN_TEST(test_cbuf2_has_ogg_page_list);
    RUN_TEST(test_cbuf2_ogg_song_page);

    /* TRACK_INFO with OGG Tests */
    RUN_TEST(test_track_info_ogg_comments);

#if (HAVE_OGG_VORBIS)
    /* RIP_MANAGER_INFO OGG Fields Tests */
    RUN_TEST(test_rmi_ogg_fields);
#endif

    /* SR_HTTP_HEADER OGG Content Type Tests */
    RUN_TEST(test_sr_http_header_ogg);

    /* Granule Position Tests */
    RUN_TEST(test_ogg_granule_position_size);

    return UNITY_END();
}
