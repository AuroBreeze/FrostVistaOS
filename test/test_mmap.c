#include "user.h"
#include "libtest.h"

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20

static void test_mmap_anonymous_one_page(void)
{
	TEST_START("test_mmap_anonymous_one_page");

	char *page = mmap(0, 4096, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(page != (void *) -1, "test_mmap_anonymous_one_page",
		    "mmap should return a mapped page");

	page[0] = 'M';
	page[4095] = 'P';
	TEST_ASSERT(page[0] == 'M' && page[4095] == 'P',
		    "test_mmap_anonymous_one_page",
		    "mapped page should be readable and writable");

	TEST_PASS("test_mmap_anonymous_one_page");
}

static void test_munmap_whole_vma(void)
{
	TEST_START("test_munmap_whole_vma");

	char *page = mmap(0, 4096, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(page != (void *) -1, "test_munmap_whole_vma",
		    "mmap should return a mapped page before munmap");

	page[0] = 'U';
	TEST_ASSERT(munmap(page, 4096) == 0, "test_munmap_whole_vma",
		    "munmap should release a whole VMA");
	TEST_ASSERT(munmap(page, 4096) == -1, "test_munmap_whole_vma",
		    "munmap should reject an already unmapped range");

	TEST_PASS("test_munmap_whole_vma");
}

static void test_munmap_trims_vma_head(void)
{
	TEST_START("test_munmap_trims_vma_head");

	char *pages = mmap(0, 8192, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(pages != (void *) -1, "test_munmap_trims_vma_head",
		    "mmap should return a two-page mapping");

	pages[4096] = 'H';
	pages[8191] = 'h';
	TEST_ASSERT(munmap(pages, 4096) == 0, "test_munmap_trims_vma_head",
		    "munmap should trim the first page from a VMA");
	TEST_ASSERT(pages[4096] == 'H' && pages[8191] == 'h',
		    "test_munmap_trims_vma_head",
		    "the remaining tail of the VMA should stay mapped");
	TEST_ASSERT(munmap(pages + 4096, 4096) == 0,
		    "test_munmap_trims_vma_head",
		    "munmap should release the remaining tail VMA");

	TEST_PASS("test_munmap_trims_vma_head");
}

static void test_munmap_trims_vma_tail(void)
{
	TEST_START("test_munmap_trims_vma_tail");

	char *pages = mmap(0, 8192, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(pages != (void *) -1, "test_munmap_trims_vma_tail",
		    "mmap should return a two-page mapping");

	pages[0] = 'T';
	pages[4095] = 't';
	TEST_ASSERT(munmap(pages + 4096, 4096) == 0,
		    "test_munmap_trims_vma_tail",
		    "munmap should trim the last page from a VMA");
	TEST_ASSERT(pages[0] == 'T' && pages[4095] == 't',
		    "test_munmap_trims_vma_tail",
		    "the remaining head of the VMA should stay mapped");
	TEST_ASSERT(munmap(pages, 4096) == 0, "test_munmap_trims_vma_tail",
		    "munmap should release the remaining head VMA");

	TEST_PASS("test_munmap_trims_vma_tail");
}

static void test_munmap_rejects_middle_split(void)
{
	TEST_START("test_munmap_rejects_middle_split");

	char *pages = mmap(0, 12288, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(pages != (void *) -1, "test_munmap_rejects_middle_split",
		    "mmap should return a three-page mapping");

	pages[0] = 'M';
	pages[4096] = 'm';
	pages[8192] = 'S';
	TEST_ASSERT(
	    munmap(pages + 4096, 4096) == -1,
	    "test_munmap_rejects_middle_split",
	    "munmap should reject middle splits until VMA split is supported");
	TEST_ASSERT(pages[0] == 'M' && pages[4096] == 'm' && pages[8192] == 'S',
		    "test_munmap_rejects_middle_split",
		    "a rejected middle split should leave the VMA mapped");
	TEST_ASSERT(
	    munmap(pages, 12288) == 0, "test_munmap_rejects_middle_split",
	    "munmap should release the original VMA after a rejected split");

	TEST_PASS("test_munmap_rejects_middle_split");
}

static void test_mmap_anonymous_two_pages(void)
{
	TEST_START("test_mmap_anonymous_two_pages");

	char *pages = mmap(0, 8192, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(pages != (void *) -1, "test_mmap_anonymous_two_pages",
		    "mmap should return a two-page mapping");

	pages[0] = 'A';
	pages[4095] = 'B';
	pages[4096] = 'C';
	pages[8191] = 'D';
	TEST_ASSERT(pages[0] == 'A' && pages[4095] == 'B' &&
			pages[4096] == 'C' && pages[8191] == 'D',
		    "test_mmap_anonymous_two_pages",
		    "both pages should be independently readable and writable");

	TEST_ASSERT(munmap(pages, 8192) == 0, "test_mmap_anonymous_two_pages",
		    "munmap should release the two-page VMA");
	TEST_PASS("test_mmap_anonymous_two_pages");
}

static void test_mmap_anonymous_zero_fill(void)
{
	TEST_START("test_mmap_anonymous_zero_fill");

	char *page = mmap(0, 4096, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(page != (void *) -1, "test_mmap_anonymous_zero_fill",
		    "mmap should return a mapped page");
	TEST_ASSERT(page[0] == 0 && page[4095] == 0,
		    "test_mmap_anonymous_zero_fill",
		    "anonymous mappings should be zero-filled");

	TEST_ASSERT(munmap(page, 4096) == 0, "test_mmap_anonymous_zero_fill",
		    "munmap should release the zero-fill test page");
	TEST_PASS("test_mmap_anonymous_zero_fill");
}

static void test_mmap_rounds_vma_length(void)
{
	TEST_START("test_mmap_rounds_vma_length");

	char *page = mmap(0, 2048, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(page != (void *) -1, "test_mmap_rounds_vma_length",
		    "mmap should accept sub-page lengths");

	page[2047] = 'A';
	page[4095] = 'B';
	TEST_ASSERT(page[2047] == 'A' && page[4095] == 'B',
		    "test_mmap_rounds_vma_length",
		    "mmap should map the rounded page length");
	TEST_ASSERT(munmap(page, 2048) == 0, "test_mmap_rounds_vma_length",
		    "munmap should release the rounded VMA length");

	TEST_PASS("test_mmap_rounds_vma_length");
}

static void test_munmap_rejects_invalid_ranges(void)
{
	TEST_START("test_munmap_rejects_invalid_ranges");

	char *page = mmap(0, 4096, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(
	    page != (void *) -1, "test_munmap_rejects_invalid_ranges",
	    "mmap should return a mapped page before invalid munmap tests");

	TEST_ASSERT(munmap(page + 1, 4096) == -1,
		    "test_munmap_rejects_invalid_ranges",
		    "munmap should reject unaligned addresses");
	TEST_ASSERT(munmap(page, 0) == -1, "test_munmap_rejects_invalid_ranges",
		    "munmap should reject zero length");
	TEST_ASSERT(munmap(page, 2048) == 0,
		    "test_munmap_rejects_invalid_ranges",
		    "munmap should round partial-page length to the whole VMA");

	TEST_PASS("test_munmap_rejects_invalid_ranges");
}

static void test_mmap_rejects_overlapping_fixed_addr(void)
{
	TEST_START("test_mmap_rejects_overlapping_fixed_addr");

	char *page = mmap(0, 4096, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(page != (void *) -1,
		    "test_mmap_rejects_overlapping_fixed_addr",
		    "mmap should return a mapped page before overlap test");

	char *overlap = mmap(page, 4096, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(overlap == (void *) -1,
		    "test_mmap_rejects_overlapping_fixed_addr",
		    "mmap should reject an overlapping fixed address");
	TEST_ASSERT(munmap(page, 4096) == 0,
		    "test_mmap_rejects_overlapping_fixed_addr",
		    "munmap should release the original mapping");

	TEST_PASS("test_mmap_rejects_overlapping_fixed_addr");
}

void _start(void)
{
	TEST_START("mmap");
	test_mmap_anonymous_one_page();
	test_munmap_whole_vma();
	test_munmap_trims_vma_head();
	test_munmap_trims_vma_tail();
	test_munmap_rejects_middle_split();
	test_mmap_anonymous_two_pages();
	test_mmap_anonymous_zero_fill();
	test_mmap_rounds_vma_length();
	test_munmap_rejects_invalid_ranges();
	test_mmap_rejects_overlapping_fixed_addr();
	TEST_PASS("mmap");
	shutdown();
}
