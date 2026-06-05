#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_RDWR 0x002
#define O_WRONLY 0x001
#define O_CREAT 0x040

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

static void test_lseek_basic(void)
{
	TEST_START("test_lseek_basic");
	int fd = open("/off", O_RDWR | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_lseek_basic", "create offset test file");
	long n = write(fd, "abcdef", 6);
	TEST_ASSERT(n == 6, "test_lseek_basic", "write initial data");

	long pos = lseek(fd, 0, SEEK_CUR);
	TEST_ASSERT(pos == 6, "test_lseek_basic", "offset after write");

	pos = lseek(fd, 2, SEEK_SET);
	TEST_ASSERT(pos == 2, "test_lseek_basic", "lseek SEEK_SET");
	char buf[4] = {0};
	n = read(fd, buf, 3);
	TEST_ASSERT(n == 3 && buf[0] == 'c' && buf[2] == 'e',
		    "test_lseek_basic", "read at lseek position");

	pos = lseek(fd, -2, SEEK_END);
	TEST_ASSERT(pos == 4, "test_lseek_basic", "lseek SEEK_END negative");
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, 3);
	TEST_ASSERT(n == 2 && buf[0] == 'e' && buf[1] == 'f',
		    "test_lseek_basic", "read from near EOF");

	pos = lseek(fd, 10, SEEK_SET);
	TEST_ASSERT(pos == 10, "test_lseek_basic", "lseek past EOF");
	n = read(fd, buf, 3);
	TEST_ASSERT(n <= 0, "test_lseek_basic", "read past EOF returns EOF");

	n = write(fd, "XY", 2);
	TEST_ASSERT(n == 2, "test_lseek_basic", "write past EOF extends file");

	pos = lseek(fd, 0, SEEK_END);
	TEST_ASSERT(pos == 12, "test_lseek_basic",
		    "file size after hole write");
	close(fd);
	TEST_PASS("test_lseek_basic");
}

static void test_lseek_dup_shares_offset(void)
{
	TEST_START("test_lseek_dup_shares_offset");
	int fd = open("/off2", O_RDWR | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_lseek_dup_shares_offset",
		    "create dup-offset file");
	long n = write(fd, "1234", 4);
	TEST_ASSERT(n == 4, "test_lseek_dup_shares_offset", "write 4 bytes");
	long pos = lseek(fd, 0, SEEK_SET);
	TEST_ASSERT(pos == 0, "test_lseek_dup_shares_offset", "lseek to start");
	n = write(fd, "AB", 2);
	TEST_ASSERT(n == 2, "test_lseek_dup_shares_offset",
		    "overwrite at offset 0");
	char buf[4] = {0};
	n = read(fd, buf, 3);
	TEST_ASSERT(n == 2 && buf[0] == '3' && buf[1] == '4',
		    "test_lseek_dup_shares_offset", "read from write position");
	close(fd);
	TEST_PASS("test_lseek_dup_shares_offset");
}

static void test_lseek_dup_persistence(void)
{
	TEST_START("test_lseek_dup_persistence");
	int fd = open("/off3", O_RDWR | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_lseek_dup_persistence",
		    "create dup-close file");
	long n = write(fd, "abcdef", 6);
	TEST_ASSERT(n == 6, "test_lseek_dup_persistence",
		    "write for dup-close");
	int fd2 = dup(fd);
	TEST_ASSERT(fd2 >= 0, "test_lseek_dup_persistence", "dup before close");
	close(fd);

	lseek(fd2, 0, SEEK_SET);
	char buf[4] = {0};
	n = read(fd2, buf, 3);
	TEST_ASSERT(n == 3 && buf[0] == 'a', "test_lseek_dup_persistence",
		    "read via dup after original closed");
	close(fd2);
	TEST_PASS("test_lseek_dup_persistence");
}

static void test_lseek_errors(void)
{
	TEST_START("test_lseek_errors");
	int fd = open("/off", O_RDONLY);
	if (fd < 0) {
		fd = open("/off", O_RDWR | O_CREAT);
		write(fd, "x", 1);
	}

	long pos = lseek(-1, 0, SEEK_SET);
	TEST_ASSERT(pos < 0, "test_lseek_errors", "lseek invalid fd fails");
	pos = lseek(fd, -1, SEEK_SET);
	TEST_ASSERT(pos < 0, "test_lseek_errors",
		    "lseek negative target fails");
	pos = lseek(fd, 0, 99);
	TEST_ASSERT(pos < 0, "test_lseek_errors", "lseek invalid whence fails");
	close(fd);
	TEST_PASS("test_lseek_errors");
}

void _start(void)
{
	TEST_START("easyfs_offset");
	test_lseek_basic();
	test_lseek_dup_shares_offset();
	test_lseek_dup_persistence();
	test_lseek_errors();
	TEST_PASS("easyfs_offset");
	shutdown();
}
