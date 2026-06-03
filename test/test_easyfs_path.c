#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040

void _start(void)
{
	TEST_START("easyfs_path");

	int fd = open("", O_RDONLY);
	TEST_ASSERT(fd < 0, "easyfs_path", "empty path open should fail");

	fd = open("/nonexist/file", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd < 0, "easyfs_path",
		    "create under missing parent should fail");

	fd = open("/alpha", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs_path", "create regular file /alpha");
	write(fd, "x", 1);
	TEST_ASSERT(close(fd) == 0, "easyfs_path", "close /alpha");

	fd = open("/alpha/sub", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd < 0, "easyfs_path",
		    "create under non-directory should fail");

	fd = open("/abcdefghijk1234", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd < 0, "easyfs_path",
		    "name at DIRSIZ boundary should fail");

	fd = open("/abcdefghijk12", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs_path",
		    "name one below DIRSIZ should succeed");
	write(fd, "ok", 2);
	TEST_ASSERT(close(fd) == 0, "easyfs_path", "close DIRSIZ-1 name file");

	fd = open("/abcdefghijk12", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs_path",
		    "recreate same name should open existing");

	fd = open("/abcdefghijk12", O_RDONLY);
	TEST_ASSERT(fd >= 0, "easyfs_path", "reopen DIRSIZ-1 name file");
	char buf[4] = {0};
	long n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 2 && buf[0] == 'o', "easyfs_path",
		    "read DIRSIZ-1 name file");
	TEST_ASSERT(close(fd) == 0, "easyfs_path", "close DIRSIZ-1 name read");

	TEST_PASS("easyfs_path");
	shutdown();
}
