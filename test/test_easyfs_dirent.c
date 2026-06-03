#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040

void _start(void)
{
	TEST_START("easyfs_dirent");

	int fd = open("/a", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs_dirent", "create /a");
	write(fd, "aaa", 3);
	TEST_ASSERT(close(fd) == 0, "easyfs_dirent", "close /a");

	fd = open("/b", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs_dirent", "create /b");
	write(fd, "bbb", 3);
	TEST_ASSERT(close(fd) == 0, "easyfs_dirent", "close /b");

	fd = open("/c", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs_dirent", "create /c");
	write(fd, "ccc", 3);
	TEST_ASSERT(close(fd) == 0, "easyfs_dirent", "close /c");

	int ret = unlink("/b");
	TEST_ASSERT(ret == 0, "easyfs_dirent", "unlink /b");

	fd = open("/b", O_RDONLY);
	TEST_ASSERT(fd < 0, "easyfs_dirent", "/b gone after unlink");

	fd = open("/new", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs_dirent", "create /new");
	write(fd, "zzz", 3);
	TEST_ASSERT(close(fd) == 0, "easyfs_dirent", "close /new");

	fd = open("/a", O_RDONLY);
	TEST_ASSERT(fd >= 0, "easyfs_dirent", "/a still exists");
	char buf[4] = {0};
	long n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 3 && buf[0] == 'a', "easyfs_dirent", "/a data intact");
	TEST_ASSERT(close(fd) == 0, "easyfs_dirent", "close /a");

	fd = open("/c", O_RDONLY);
	TEST_ASSERT(fd >= 0, "easyfs_dirent", "/c still exists");
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 3 && buf[0] == 'c', "easyfs_dirent", "/c data intact");
	TEST_ASSERT(close(fd) == 0, "easyfs_dirent", "close /c");

	fd = open("/new", O_RDONLY);
	TEST_ASSERT(fd >= 0, "easyfs_dirent", "/new should reuse /b's slot");
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 3 && buf[0] == 'z', "easyfs_dirent",
		    "/new data correct");
	TEST_ASSERT(close(fd) == 0, "easyfs_dirent", "close /new");

	TEST_PASS("easyfs_dirent");
	shutdown();
}
