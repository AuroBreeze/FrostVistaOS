#include "user/user.h"
#include "user/libtest.h"

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define PGSIZE 4096

/* Use an existing lower file without creating or truncating it. The first
 * shared munmap must copy it up; later opens must find the tmpfs upper file. */
static const char *path = "/musl/basic/text.txt";

static void check_contents(int fd, const char *expected, long file_size)
{
	char actual[256];
	TEST_ASSERT(lseek(fd, 0, 0) == 0, "mmap_overlay",
		    "should seek to the beginning of the file");
	TEST_ASSERT(read(fd, actual, sizeof(actual)) == file_size,
		    "mmap_overlay", "writeback must preserve file length");
	for (long i = 0; i < file_size; i++)
		TEST_ASSERT(
		    actual[i] == expected[i], "mmap_overlay",
		    "file bytes should match mapped edits and untouched data");
}

static void test_private_does_not_copy_up(char *expected, long file_size)
{
	TEST_START("test_private_does_not_copy_up");
	int fd = open(path, O_RDONLY);
	TEST_ASSERT(fd >= 0, "mmap_overlay", "should open lower file");
	char *page =
	    mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	TEST_ASSERT((long) page > 0, "mmap_overlay",
		    "should map lower file privately");
	page[0] ^= 1;
	TEST_ASSERT(munmap(page, file_size) == 0, "mmap_overlay",
		    "private munmap should not write back");
	check_contents(fd, expected, file_size);
	TEST_ASSERT(close(fd) == 0, "mmap_overlay", "should close private fd");
	TEST_PASS("test_private_does_not_copy_up");
}

static void test_shared_copy_up(char *expected, long file_size)
{
	TEST_START("test_shared_copy_up");
	int fd = open(path, O_RDWR);
	TEST_ASSERT(fd >= 0, "mmap_overlay",
		    "should open lower file for mapping");
	char *page =
	    mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	TEST_ASSERT((long) page > 0, "mmap_overlay",
		    "should map lower file for shared writes");
	expected[1] ^= 1;
	expected[file_size - 1] ^= 1;
	page[1] = expected[1];
	page[file_size - 1] = expected[file_size - 1];
	page[file_size] = 'X';
	TEST_ASSERT(lseek(fd, 7, 0) == 7, "mmap_overlay",
		    "should set an independent open-file offset");
	TEST_ASSERT(munmap(page, file_size) == 0, "mmap_overlay",
		    "shared munmap should copy up and write back");
	TEST_ASSERT(lseek(fd, 0, 1) == 7, "mmap_overlay",
		    "copy-up writeback must preserve the open-file offset");
	check_contents(fd, expected, file_size);
	TEST_ASSERT(close(fd) == 0, "mmap_overlay",
		    "should close copied-up fd");

	fd = open(path, O_RDONLY);
	TEST_ASSERT(fd >= 0, "mmap_overlay", "should reopen copied-up file");
	check_contents(fd, expected, file_size);
	TEST_ASSERT(close(fd) == 0, "mmap_overlay", "should close reopened fd");
	TEST_PASS("test_shared_copy_up");
}

static void test_shared_after_close(char *expected, long file_size)
{
	TEST_START("test_shared_after_close");
	/* Repeatedly release the fd before munmap to exercise VMA ownership of
	 * the copied-up inode and subsequent lookup of the same upper file. */
	for (int i = 0; i < 8; i++) {
		int fd = open(path, O_RDWR);
		TEST_ASSERT(fd >= 0, "mmap_overlay",
			    "should reopen upper file");
		char *page = mmap(0, file_size, PROT_READ | PROT_WRITE,
				  MAP_SHARED, fd, 0);
		TEST_ASSERT((long) page > 0, "mmap_overlay",
			    "should map upper file");
		TEST_ASSERT(close(fd) == 0, "mmap_overlay",
			    "should close fd before fault and writeback");
		expected[2] = 'a' + i;
		page[2] = expected[2];
		TEST_ASSERT(munmap(page, file_size) == 0, "mmap_overlay",
			    "VMA should retain the file through writeback");
		fd = open(path, O_RDONLY);
		TEST_ASSERT(fd >= 0, "mmap_overlay",
			    "should reopen after munmap");
		check_contents(fd, expected, file_size);
		TEST_ASSERT(close(fd) == 0, "mmap_overlay",
			    "should close reader");
	}
	TEST_PASS("test_shared_after_close");
}

int main(int argc, char **argv)
{
	TEST_START("mmap_overlay");
	int fd = open(path, O_RDONLY);
	TEST_ASSERT(fd >= 0, "mmap_overlay", "ext4 text fixture must exist");
	char expected[256];
	long file_size = read(fd, expected, sizeof(expected));
	TEST_ASSERT(
	    file_size > 7 && file_size < sizeof(expected), "mmap_overlay",
	    "fixture must fit in the buffer and leave a partial final page");
	TEST_ASSERT(close(fd) == 0, "mmap_overlay",
		    "should close fixture reader");
	test_private_does_not_copy_up(expected, file_size);
	test_shared_copy_up(expected, file_size);
	test_shared_after_close(expected, file_size);
	TEST_PASS("mmap_overlay");
	shutdown();
}
