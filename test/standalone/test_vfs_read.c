#include "user/user.h"
#include "user/libtest.h"
#include "asm/user.h"

#define PGSIZE 4096
#define FILE_SIZE (PGSIZE * 2 + 37)
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_SHARED 1
#define SEEK_SET 0
#define SEEK_CUR 1
#define SYS_READV 65

static char expected[FILE_SIZE];
static char data[FILE_SIZE + 32];

struct test_iovec {
	void *base;
	uint64 len;
};

static void check_data(const char *buf, int offset, int count)
{
	for (int i = 0; i < count; i++)
		TEST_ASSERT(
		    buf[i] == expected[offset + i], "vfs_read",
		    "read bytes should match the requested file offset");
}

static void check_offset(int fd, long offset)
{
	TEST_ASSERT(lseek(fd, 0, SEEK_CUR) == offset, "vfs_read",
		    "file offset should advance by exactly the bytes read");
}

static void test_read_offsets(int fd)
{
	TEST_START("test_read_offsets");
	TEST_ASSERT(lseek(fd, 0, SEEK_SET) == 0, "test_read_offsets",
		    "should seek to the start");
	TEST_ASSERT(read(fd, data, 17) == 17, "test_read_offsets",
		    "first sequential read should succeed");
	check_data(data, 0, 17);
	check_offset(fd, 17);
	TEST_ASSERT(read(fd, data, 29) == 29, "test_read_offsets",
		    "second read should continue from the previous position");
	check_data(data, 17, 29);
	check_offset(fd, 46);

	TEST_ASSERT(lseek(fd, PGSIZE - 7, SEEK_SET) == PGSIZE - 7,
		    "test_read_offsets", "should seek near a page boundary");
	TEST_ASSERT(read(fd, data, PGSIZE + 25) == PGSIZE + 25,
		    "test_read_offsets",
		    "unaligned read should span three pages");
	check_data(data, PGSIZE - 7, PGSIZE + 25);
	check_offset(fd, PGSIZE * 2 + 18);

	TEST_ASSERT(lseek(fd, FILE_SIZE - 5, SEEK_SET) == FILE_SIZE - 5,
		    "test_read_offsets", "should seek near EOF");
	memset(data, '?', 32);
	TEST_ASSERT(read(fd, data, 32) == 5, "test_read_offsets",
		    "read should stop at EOF");
	check_data(data, FILE_SIZE - 5, 5);
	TEST_ASSERT(data[5] == '?', "test_read_offsets",
		    "short read should leave the remaining buffer unchanged");
	check_offset(fd, FILE_SIZE);
	TEST_ASSERT(read(fd, data, 1) == 0, "test_read_offsets",
		    "read at EOF should return zero");
	check_offset(fd, FILE_SIZE);
	TEST_ASSERT(lseek(fd, FILE_SIZE + 9, SEEK_SET) == FILE_SIZE + 9,
		    "test_read_offsets", "should seek beyond EOF");
	TEST_ASSERT(read(fd, data, 1) == 0, "test_read_offsets",
		    "read beyond EOF should return zero");
	check_offset(fd, FILE_SIZE + 9);
	TEST_PASS("test_read_offsets");
}

static void test_readv_offsets(int fd)
{
	TEST_START("test_readv_offsets");
	struct test_iovec iov[] = {
	    {data, 19},
	    {data + 19, PGSIZE},
	    {data + 19 + PGSIZE, 27},
	};
	int start = PGSIZE - 11;
	int count = 19 + PGSIZE + 27;
	TEST_ASSERT(lseek(fd, start, SEEK_SET) == start, "test_readv_offsets",
		    "should position vectored read");
	TEST_ASSERT(arch_user_syscall3(SYS_READV, fd, (long) iov, 3) == count,
		    "test_readv_offsets", "readv should span all vectors");
	check_data(data, start, count);
	check_offset(fd, start + count);
	TEST_PASS("test_readv_offsets");
}

static void test_cached_reads(int fd)
{
	TEST_START("test_cached_reads");
	/* Cache only the middle page, leaving both adjacent pages uncached. */
	volatile char *page =
	    mmap(0, PGSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PGSIZE);
	TEST_ASSERT((long) page > 0, "test_cached_reads",
		    "should map the middle file page");
	page[0] = 'X';
	page[PGSIZE - 1] = 'Y';
	expected[PGSIZE] = 'X';
	expected[PGSIZE * 2 - 1] = 'Y';

	int reader = open("/vfs-read", O_RDONLY);
	TEST_ASSERT(reader >= 0, "test_cached_reads",
		    "should open an independent reader");
	volatile char *other =
	    mmap(0, PGSIZE, PROT_READ, MAP_SHARED, reader, PGSIZE);
	TEST_ASSERT((long) other > 0, "test_cached_reads",
		    "should map the same file through another open");
	TEST_ASSERT(other[0] == 'X' && other[PGSIZE - 1] == 'Y',
		    "test_cached_reads",
		    "separate mappings should share writes");
	check_offset(reader, 0);
	TEST_ASSERT(lseek(reader, PGSIZE, SEEK_SET) == PGSIZE,
		    "test_cached_reads", "should seek to cached data");
	TEST_ASSERT(read(reader, data, 1) == 1 && data[0] == 'X',
		    "test_cached_reads",
		    "read should see writes before munmap");
	check_offset(reader, PGSIZE + 1);
	check_offset(fd, FILE_SIZE + 9);

	test_read_offsets(reader);
	/* Request two bytes past EOF in the final vector. */
	struct test_iovec iov[] = {
	    {data, 19}, {data + 19, PGSIZE}, {data + 19 + PGSIZE, 31}};
	TEST_ASSERT(lseek(reader, PGSIZE - 11, SEEK_SET) == PGSIZE - 11,
		    "test_cached_reads", "should position cached readv");
	int count = FILE_SIZE - (PGSIZE - 11);
	TEST_ASSERT(arch_user_syscall3(SYS_READV, reader, (long) iov, 3) ==
			count,
		    "test_cached_reads", "cached readv should stop at EOF");
	check_data(data, PGSIZE - 11, count);
	check_offset(reader, FILE_SIZE);
	TEST_ASSERT(munmap((void *) other, PGSIZE) == 0, "test_cached_reads",
		    "should unmap the reader view");
	TEST_ASSERT(close(reader) == 0, "test_cached_reads",
		    "should close the reader");
	TEST_ASSERT(munmap((void *) page, PGSIZE) == 0, "test_cached_reads",
		    "should unmap and write back the shared page");
	TEST_PASS("test_cached_reads");
}

int main(int argc, char **argv)
{
	TEST_START("vfs_read");
	for (int i = 0; i < FILE_SIZE; i++)
		expected[i] = 'a' + ((i * 7 + i / PGSIZE) % 26);
	int fd = open("/vfs-read", O_RDWR | O_CREAT | O_TRUNC);
	TEST_ASSERT(fd >= 0, "vfs_read", "should create the backing file");
	TEST_ASSERT(write(fd, expected, FILE_SIZE) == FILE_SIZE, "vfs_read",
		    "should write the complete backing file");
	test_read_offsets(fd);
	test_readv_offsets(fd);
	/* Keep a known independent offset while the other descriptor reads. */
	TEST_ASSERT(lseek(fd, FILE_SIZE + 9, SEEK_SET) == FILE_SIZE + 9,
		    "vfs_read", "should position the mapping descriptor");
	test_cached_reads(fd);
	TEST_ASSERT(close(fd) == 0, "vfs_read",
		    "should close the backing file");
	TEST_ASSERT(unlinkat(AT_FDCWD, "/vfs-read", 0) == 0, "vfs_read",
		    "should remove the backing file");
	TEST_PASS("vfs_read");
	shutdown();
}
