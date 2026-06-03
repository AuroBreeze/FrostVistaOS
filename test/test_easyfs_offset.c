#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_RDWR 0x002
#define O_WRONLY 0x001
#define O_CREAT 0x040

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

void _start(void)
{
	TEST_START("easyfs_offset");

	int fd = open("/off", O_RDWR | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs_offset", "create offset test file");
	long n = write(fd, "abcdef", 6);
	TEST_ASSERT(n == 6, "easyfs_offset", "write initial data");

	long pos = lseek(fd, 0, SEEK_CUR);
	TEST_ASSERT(pos == 6, "easyfs_offset", "offset after write");

	pos = lseek(fd, 2, SEEK_SET);
	TEST_ASSERT(pos == 2, "easyfs_offset", "lseek SEEK_SET");
	char buf[4] = {0};
	n = read(fd, buf, 3);
	TEST_ASSERT(n == 3 && buf[0] == 'c' && buf[2] == 'e', "easyfs_offset",
		    "read at lseek position");

	pos = lseek(fd, -2, SEEK_END);
	TEST_ASSERT(pos == 4, "easyfs_offset", "lseek SEEK_END negative");
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, 3);
	TEST_ASSERT(n == 2 && buf[0] == 'e' && buf[1] == 'f', "easyfs_offset",
		    "read from near EOF");

	pos = lseek(fd, 10, SEEK_SET);
	TEST_ASSERT(pos == 10, "easyfs_offset", "lseek past EOF");
	n = read(fd, buf, 3);
	TEST_ASSERT(n <= 0, "easyfs_offset", "read past EOF returns EOF");

	n = write(fd, "XY", 2);
	TEST_ASSERT(n == 2, "easyfs_offset", "write past EOF extends file");

	pos = lseek(fd, 0, SEEK_END);
	TEST_ASSERT(pos == 12, "easyfs_offset", "file size after hole write");

	int fd2 = dup(fd);
	TEST_ASSERT(fd2 >= 0, "easyfs_offset", "dup offset test fd");

	pos = lseek(fd2, 0, SEEK_SET);
	TEST_ASSERT(pos == 0, "easyfs_offset", "dup shares offset");

	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, 2);
	TEST_ASSERT(n == 2 && buf[0] == 'a' && buf[1] == 'b', "easyfs_offset",
		    "read via original fd");

	memset(buf, 0, sizeof(buf));
	n = read(fd2, buf, 2);
	TEST_ASSERT(n == 2 && buf[0] == 'c' && buf[1] == 'd', "easyfs_offset",
		    "dup shares offset after read");

	TEST_ASSERT(close(fd) == 0, "easyfs_offset", "close original fd");
	TEST_ASSERT(close(fd2) == 0, "easyfs_offset", "close dup fd");

	fd = open("/off2", O_RDWR | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs_offset", "create second offset file");
	n = write(fd, "1234", 4);
	TEST_ASSERT(n == 4, "easyfs_offset", "write 4 bytes");
	pos = lseek(fd, 0, SEEK_SET);
	TEST_ASSERT(pos == 0, "easyfs_offset", "lseek to start");
	n = write(fd, "AB", 2);
	TEST_ASSERT(n == 2, "easyfs_offset", "overwrite at offset 0");
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, 3);
	TEST_ASSERT(n == 2 && buf[0] == '3' && buf[1] == '4', "easyfs_offset",
		    "read from write position");
	TEST_ASSERT(close(fd) == 0, "easyfs_offset", "close overwrite file");

	fd = open("/off3", O_RDWR | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs_offset", "create dup-close file");
	n = write(fd, "abcdef", 6);
	TEST_ASSERT(n == 6, "easyfs_offset", "write for dup-close");
	fd2 = dup(fd);
	TEST_ASSERT(fd2 >= 0, "easyfs_offset", "dup before close");
	TEST_ASSERT(close(fd) == 0, "easyfs_offset",
		    "close original, dup stays open");
	pos = lseek(fd2, 0, SEEK_SET);
	memset(buf, 0, sizeof(buf));
	n = read(fd2, buf, 3);
	TEST_ASSERT(n == 3 && buf[0] == 'a', "easyfs_offset",
		    "read via dup after original closed");
	TEST_ASSERT(close(fd2) == 0, "easyfs_offset", "close dup");

	pos = lseek(-1, 0, SEEK_SET);
	TEST_ASSERT(pos < 0, "easyfs_offset", "lseek invalid fd fails");
	pos = lseek(fd, -1, SEEK_SET);
	TEST_ASSERT(pos < 0, "easyfs_offset", "lseek negative target fails");
	pos = lseek(fd, 0, 99);
	TEST_ASSERT(pos < 0, "easyfs_offset", "lseek invalid whence fails");

	TEST_PASS("easyfs_offset");
	shutdown();
}
