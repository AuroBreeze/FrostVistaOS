#include "user.h"
#include "libtest.h"

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02

#define PGSIZE 4096

static char file_data[PGSIZE + 16];

static void create_mmap_file(void)
{
	for (int i = 0; i < PGSIZE + 16; i++)
		file_data[i] = 'a' + (i % 26);
	file_data[0] = 'F';
	file_data[PGSIZE - 1] = 'P';
	file_data[PGSIZE] = 'S';
	file_data[PGSIZE + 15] = 'E';

	int fd = open("/mmap-file", O_WRONLY | O_CREAT | O_TRUNC);
	TEST_ASSERT(fd >= 0, "create_mmap_file",
		    "should create mmap backing file");
	long n = write(fd, file_data, sizeof(file_data));
	TEST_ASSERT(n == sizeof(file_data), "create_mmap_file",
		    "should write mmap backing file contents");
	TEST_ASSERT(close(fd) == 0, "create_mmap_file",
		    "should close mmap backing file after writing");
}

static void test_mmap_file_private_read(void)
{
	TEST_START("test_mmap_file_private_read");

	int fd = open("/mmap-file", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_mmap_file_private_read",
		    "should open mmap backing file read-only");

	char *pages = mmap(0, PGSIZE * 2, PROT_READ, MAP_PRIVATE, fd, 0);
	TEST_ASSERT(pages != (void *) -1, "test_mmap_file_private_read",
		    "private read-only file mmap should succeed");
	TEST_ASSERT(close(fd) == 0, "test_mmap_file_private_read",
		    "closing fd after mmap should succeed");

	TEST_ASSERT(pages[0] == 'F' && pages[PGSIZE - 1] == 'P',
		    "test_mmap_file_private_read",
		    "first mapped page should expose file contents");
	TEST_ASSERT(pages[PGSIZE] == 'S' && pages[PGSIZE + 15] == 'E',
		    "test_mmap_file_private_read",
		    "second mapped page should use the fault page file offset");
	TEST_ASSERT(pages[PGSIZE + 16] == 0 && pages[PGSIZE * 2 - 1] == 0,
		    "test_mmap_file_private_read",
		    "bytes past EOF in the mapped page should be zero-filled");

	TEST_ASSERT(munmap(pages, PGSIZE * 2) == 0,
		    "test_mmap_file_private_read",
		    "munmap should release file-backed mapping");
	TEST_PASS("test_mmap_file_private_read");
}

static void test_mmap_file_fault_order_is_address_based(void)
{
	TEST_START("test_mmap_file_fault_order_is_address_based");

	int fd = open("/mmap-file", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_mmap_file_fault_order_is_address_based",
		    "should open mmap backing file read-only");

	char *pages = mmap(0, PGSIZE * 2, PROT_READ, MAP_PRIVATE, fd, 0);
	TEST_ASSERT(pages != (void *) -1,
		    "test_mmap_file_fault_order_is_address_based",
		    "file mmap should succeed before out-of-order faults");
	TEST_ASSERT(close(fd) == 0,
		    "test_mmap_file_fault_order_is_address_based",
		    "closing fd after mmap should succeed");

	TEST_ASSERT(
	    pages[PGSIZE] == 'S' && pages[PGSIZE + 15] == 'E',
	    "test_mmap_file_fault_order_is_address_based",
	    "faulting the second page first should read the second file page");
	TEST_ASSERT(pages[0] == 'F' && pages[PGSIZE - 1] == 'P',
		    "test_mmap_file_fault_order_is_address_based",
		    "faulting the first page later should still read the first "
		    "file page");

	TEST_ASSERT(munmap(pages, PGSIZE * 2) == 0,
		    "test_mmap_file_fault_order_is_address_based",
		    "munmap should release out-of-order fault mapping");
	TEST_PASS("test_mmap_file_fault_order_is_address_based");
}

static void test_mmap_file_invalid_fd_does_not_consume_vma(void)
{
	TEST_START("test_mmap_file_invalid_fd_does_not_consume_vma");

	int fd = open("/mmap-file", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_mmap_file_invalid_fd_does_not_consume_vma",
		    "should open mmap backing file before closed-fd checks");
	TEST_ASSERT(close(fd) == 0,
		    "test_mmap_file_invalid_fd_does_not_consume_vma",
		    "closing fd before invalid mmap checks should succeed");

	for (int i = 0; i < 20; i++) {
		char *mapping = mmap(0, PGSIZE, PROT_READ, MAP_PRIVATE, fd, 0);
		TEST_ASSERT(mapping == (void *) -1,
			    "test_mmap_file_invalid_fd_does_not_consume_vma",
			    "mmap with a closed fd should fail");
	}

	fd = open("/mmap-file", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_mmap_file_invalid_fd_does_not_consume_vma",
		    "should reopen backing file after invalid mmap attempts");
	char *pages = mmap(0, PGSIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	TEST_ASSERT(pages != (void *) -1,
		    "test_mmap_file_invalid_fd_does_not_consume_vma",
		    "failed mmap attempts should not consume VMA slots");
	TEST_ASSERT(
	    pages[0] == 'F', "test_mmap_file_invalid_fd_does_not_consume_vma",
	    "valid mmap after invalid attempts should read file contents");
	TEST_ASSERT(munmap(pages, PGSIZE) == 0,
		    "test_mmap_file_invalid_fd_does_not_consume_vma",
		    "munmap should release mapping after invalid fd checks");
	TEST_ASSERT(close(fd) == 0,
		    "test_mmap_file_invalid_fd_does_not_consume_vma",
		    "closing reopened backing file should succeed");

	TEST_PASS("test_mmap_file_invalid_fd_does_not_consume_vma");
}

static void test_mmap_file_head_trim_preserves_file_offset(void)
{
	TEST_START("test_mmap_file_head_trim_preserves_file_offset");

	int fd = open("/mmap-file", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_mmap_file_head_trim_preserves_file_offset",
		    "should open mmap backing file read-only");

	char *pages = mmap(0, PGSIZE * 2, PROT_READ, MAP_PRIVATE, fd, 0);
	TEST_ASSERT(pages != (void *) -1,
		    "test_mmap_file_head_trim_preserves_file_offset",
		    "file mmap should succeed before head trim");
	TEST_ASSERT(close(fd) == 0,
		    "test_mmap_file_head_trim_preserves_file_offset",
		    "closing fd after mmap should succeed");

	TEST_ASSERT(munmap(pages, PGSIZE) == 0,
		    "test_mmap_file_head_trim_preserves_file_offset",
		    "munmap should trim first file-backed page");
	TEST_ASSERT(pages[PGSIZE] == 'S' && pages[PGSIZE + 15] == 'E',
		    "test_mmap_file_head_trim_preserves_file_offset",
		    "remaining VMA should keep the matching file offset");
	TEST_ASSERT(munmap(pages + PGSIZE, PGSIZE) == 0,
		    "test_mmap_file_head_trim_preserves_file_offset",
		    "munmap should release remaining trimmed mapping");

	TEST_PASS("test_mmap_file_head_trim_preserves_file_offset");
}

static void test_mmap_file_rejects_unsupported_modes(void)
{
	TEST_START("test_mmap_file_rejects_unsupported_modes");

	int fd = open("/mmap-file", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_mmap_file_rejects_unsupported_modes",
		    "should open mmap backing file read-only");

	char *mapping =
	    mmap(0, PGSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	TEST_ASSERT(mapping == (void *) -1,
		    "test_mmap_file_rejects_unsupported_modes",
		    "writable file-backed mmap should be rejected");

	mapping = mmap(0, PGSIZE, PROT_READ, MAP_SHARED, fd, 0);
	TEST_ASSERT(mapping == (void *) -1,
		    "test_mmap_file_rejects_unsupported_modes",
		    "shared file-backed mmap should be rejected");

	mapping = mmap(0, PGSIZE, PROT_READ, MAP_PRIVATE, fd, 1);
	TEST_ASSERT(mapping == (void *) -1,
		    "test_mmap_file_rejects_unsupported_modes",
		    "unaligned file offset should be rejected");

	TEST_ASSERT(close(fd) == 0, "test_mmap_file_rejects_unsupported_modes",
		    "closing backing file should succeed");
	TEST_PASS("test_mmap_file_rejects_unsupported_modes");
}

void _start(void)
{
	TEST_START("mmap_file");
	create_mmap_file();
	test_mmap_file_private_read();
	test_mmap_file_fault_order_is_address_based();
	test_mmap_file_invalid_fd_does_not_consume_vma();
	test_mmap_file_head_trim_preserves_file_offset();
	test_mmap_file_rejects_unsupported_modes();
	TEST_PASS("mmap_file");
	shutdown();
}
