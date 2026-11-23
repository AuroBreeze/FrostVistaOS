#include "user/user.h"
#include "user/libtest.h"

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
		TEST_ASSERT(mapping == (void *) -EBADF,
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
	TEST_ASSERT((long) mapping > 0,
		    "test_mmap_file_rejects_unsupported_modes",
		    "private writable mmap should accept a read-only fd");
	mapping[0] = 'X';
	TEST_ASSERT(munmap(mapping, PGSIZE) == 0,
		    "test_mmap_file_rejects_unsupported_modes",
		    "private writable mapping should unmap without writeback");

	mapping = mmap(0, PGSIZE, PROT_READ, MAP_SHARED, fd, 0);
	TEST_ASSERT(
	    (long) mapping > 0 && mapping[0] == 'F',
	    "test_mmap_file_rejects_unsupported_modes",
	    "shared read-only mapping should retain original file data");
	TEST_ASSERT(munmap(mapping, PGSIZE) == 0,
		    "test_mmap_file_rejects_unsupported_modes",
		    "shared read-only mapping should unmap without writing");

	mapping = mmap(0, PGSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	TEST_ASSERT(mapping == (void *) -EACCES,
		    "test_mmap_file_rejects_unsupported_modes",
		    "shared writable mmap should reject a read-only fd");

	mapping = mmap(0, PGSIZE, PROT_READ, MAP_PRIVATE, fd, 1);
	TEST_ASSERT(mapping == (void *) -EINVAL,
		    "test_mmap_file_rejects_unsupported_modes",
		    "unaligned file offset should be rejected");

	TEST_ASSERT(close(fd) == 0, "test_mmap_file_rejects_unsupported_modes",
		    "closing backing file should succeed");
	TEST_PASS("test_mmap_file_rejects_unsupported_modes");
}

static void test_mmap_file_partial_unmap_releases_lock(void)
{
	const char *name = "test_mmap_file_partial_unmap_releases_lock";
	TEST_START(name);

	int fd = open("/mmap-file", O_RDWR);
	TEST_ASSERT(fd >= 0, name, "should open writable backing file");
	for (int trim_head = 0; trim_head <= 1; trim_head++) {
		char *pages = mmap(0, PGSIZE * 2, PROT_READ | PROT_WRITE,
				   MAP_SHARED, fd, 0);
		TEST_ASSERT((long) pages > 0, name, "should map two file pages");

		char *removed = pages + (trim_head ? 0 : PGSIZE);
		char *kept = pages + (trim_head ? PGSIZE : 0);
		*removed = trim_head ? 'F' : 'S';
		TEST_ASSERT(munmap(removed, PGSIZE) == 0, name,
			    "should unmap one edge of a writable shared mapping");

		// Faulting the remaining page must reacquire the inode operation lock.
		TEST_ASSERT(*kept == (trim_head ? 'S' : 'F'), name,
			    "should fault in the remaining page after partial unmap");
		TEST_ASSERT(munmap(kept, PGSIZE) == 0, name,
			    "should unmap the remaining page");
	}
	TEST_ASSERT(close(fd) == 0, name, "should close the backing file");
	TEST_PASS(name);
}

static void test_mmap_file_last_mapping_reference_releases_lock(void)
{
	const char *name = "test_mmap_file_last_mapping_reference_releases_lock";
	TEST_START(name);

	int fd = open("/mmap-file", O_RDWR);
	int probe_fd = open("/mmap-file", O_RDWR);
	TEST_ASSERT(fd >= 0 && probe_fd >= 0, name,
		    "should open separate descriptions for the same inode");
	char *page = mmap(0, PGSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	TEST_ASSERT((long) page > 0, name, "should map the file");
	TEST_ASSERT(close(fd) == 0, name,
		    "should leave the mapping as the last file reference");
	*page = 'L';
	TEST_ASSERT(munmap(page, PGSIZE) == 0, name,
		    "should unmap and close the original file description");

	char byte;
	TEST_ASSERT(read(probe_fd, &byte, 1) == 1 && byte == 'L', name,
		    "should read back the mapped write");
	TEST_ASSERT(lseek(probe_fd, 0, 0) == 0 && write(probe_fd, "F", 1) == 1,
		    name, "should acquire the same inode lock for a new write");
	TEST_ASSERT(close(probe_fd) == 0, name, "should close the second fd");
	TEST_PASS(name);
}

static void test_mmap_file_shared_writeback(void)
{
	TEST_START("test_mmap_file_shared_writeback");
	int fd = open("/mmap-file", O_RDWR);
	TEST_ASSERT(fd >= 0, "test_mmap_file_shared_writeback",
		    "should open writable backing file");
	char *page =
	    mmap(0, 16, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PGSIZE);
	TEST_ASSERT((long) page > 0, "test_mmap_file_shared_writeback",
		    "should map a partial final page at a nonzero file offset");
	page[0] = 'W';
	page[15] = 'Z';
	page[16] = 'X';
	TEST_ASSERT(lseek(fd, 7, 0) == 7, "test_mmap_file_shared_writeback",
		    "should position fd independently of mapping");
	TEST_ASSERT(
	    munmap(page, 16) == 0, "test_mmap_file_shared_writeback",
	    "shared mapping should write back before releasing its page");
	TEST_ASSERT(lseek(fd, 0, 1) == 7, "test_mmap_file_shared_writeback",
		    "writeback should preserve the open-file offset");
	TEST_ASSERT(lseek(fd, PGSIZE, 0) == PGSIZE,
		    "test_mmap_file_shared_writeback",
		    "should seek to modified data");
	char data[17];
	TEST_ASSERT(read(fd, data, sizeof(data)) == 16,
		    "test_mmap_file_shared_writeback",
		    "writeback should not extend the file past EOF");
	TEST_ASSERT(
	    data[0] == 'W' && data[15] == 'Z',
	    "test_mmap_file_shared_writeback",
	    "file should contain the mapped writes at the correct offset");
	TEST_ASSERT(close(fd) == 0, "test_mmap_file_shared_writeback",
		    "should close backing file");
	TEST_PASS("test_mmap_file_shared_writeback");
}

static void test_mmap_file_truncate_invalidates_pages(void)
{
	const char *name = "test_mmap_file_truncate_invalidates_pages";
	TEST_START(name);

	int fd = open("/mmap-file", O_RDONLY);
	TEST_ASSERT(fd >= 0, name, "should open the backing file");
	volatile char *private = mmap(0, PGSIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	volatile char *shared = mmap(0, PGSIZE, PROT_READ, MAP_SHARED, fd, 0);
	TEST_ASSERT((long) private > 0 && (long) shared > 0, name,
		    "should create private and shared mappings");
	TEST_ASSERT(private[0] == 'F' && shared[0] == 'F', name,
		    "should fault in the original file pages");
	TEST_ASSERT(close(fd) == 0, name, "should close the original fd");

	fd = open("/mmap-file", O_RDWR | O_TRUNC);
	TEST_ASSERT(fd >= 0, name, "should reopen and truncate the file");
	char byte;
	TEST_ASSERT(read(fd, &byte, 1) == 0, name,
		    "truncated file should be empty");
	TEST_ASSERT(write(fd, "N", 1) == 1, name,
		    "should write new content after truncation");
	TEST_ASSERT(private[0] == 'N' && private[1] == 0, name,
		    "private mapping should reload the new page");
	TEST_ASSERT(shared[0] == 'N' && shared[1] == 0, name,
		    "shared mapping should reload the new page without stale data");

	TEST_ASSERT(munmap((void *) private, PGSIZE) == 0, name,
		    "should unmap the private page");
	TEST_ASSERT(munmap((void *) shared, PGSIZE) == 0, name,
		    "should unmap the shared page");
	TEST_ASSERT(close(fd) == 0, name, "should close the truncated file");
	TEST_PASS(name);
}

static void test_mmap_file_high_page_cache(void)
{
	const char *name = "test_mmap_file_high_page_cache";
	const uint64 offset = 16 * PGSIZE;
	TEST_START(name);

	int fd = open("/mmap-high-page", O_RDWR | O_CREAT | O_TRUNC);
	TEST_ASSERT(fd >= 0, name, "should create a backing file");
	for (uint64 pos = 0; pos <= offset; pos += PGSIZE) {
		TEST_ASSERT(write(fd, file_data, PGSIZE) == PGSIZE, name,
			    "should populate the file through page 17");
	}

	volatile char *pages = mmap(0, offset + PGSIZE, PROT_READ | PROT_WRITE,
				    MAP_SHARED, fd, 0);
	volatile char *alias = mmap(0, PGSIZE, PROT_READ, MAP_SHARED, fd, offset);
	volatile char *private_page =
	    mmap(0, PGSIZE, PROT_READ, MAP_PRIVATE, fd, offset);
	TEST_ASSERT((long) pages > 0 && (long) alias > 0 &&
			(long) private_page > 0,
		    name, "should map the same high file page in three views");
	TEST_ASSERT(alias[0] == 'F' && pages[offset] == 'F' && pages[0] == 'F' &&
			private_page[0] == 'F',
		    name, "should fault high pages before low pages");

	pages[offset] = 'S';
	TEST_ASSERT(alias[0] == 'S', name, "shared views should use the same page");
	char byte;
	TEST_ASSERT(lseek(fd, offset, 0) == offset && read(fd, &byte, 1) == 1 &&
			byte == 'S',
		    name, "read should see the cached change at the high offset");
	TEST_ASSERT(lseek(fd, offset, 0) == offset && write(fd, "W", 1) == 1,
		    name, "should write to the cached high page through the fd");
	TEST_ASSERT(pages[offset] == 'W' && alias[0] == 'W' && private_page[0] == 'F',
		    name, "write should update shared views and preserve the private page");

	TEST_ASSERT(munmap((void *) pages, offset + PGSIZE) == 0, name,
		    "should unmap the shared range");
	TEST_ASSERT(munmap((void *) alias, PGSIZE) == 0, name,
		    "should unmap the shared alias");
	TEST_ASSERT(munmap((void *) private_page, PGSIZE) == 0, name,
		    "should unmap the private page");
	TEST_ASSERT(close(fd) == 0, name, "should close the backing file");
	TEST_PASS(name);
}

int main(int argc, char **argv)
{
	TEST_START("mmap_file");
	create_mmap_file();
	test_mmap_file_private_read();
	test_mmap_file_fault_order_is_address_based();
	test_mmap_file_invalid_fd_does_not_consume_vma();
	test_mmap_file_head_trim_preserves_file_offset();
	test_mmap_file_rejects_unsupported_modes();
	test_mmap_file_last_mapping_reference_releases_lock();
	test_mmap_file_partial_unmap_releases_lock();
	test_mmap_file_shared_writeback();
	test_mmap_file_truncate_invalidates_pages();
	test_mmap_file_high_page_cache();
	TEST_PASS("mmap_file");
	shutdown();
}
