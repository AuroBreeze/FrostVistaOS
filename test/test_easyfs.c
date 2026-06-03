#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040
#define O_TRUNC 0x200
#define O_APPEND 0x400

void _start(void)
{
	TEST_START("easyfs");

	int fd = open("/alpha", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs", "create regular file");
	long n = write(fd, "hello", 5);
	TEST_ASSERT(n == 5, "easyfs", "write regular file");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close regular file");

	fd = open("/alpha", O_RDONLY);
	TEST_ASSERT(fd >= 0, "easyfs", "reopen regular file read-only");
	char buf[8] = {0};
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 5 && buf[0] == 'h' && buf[4] == 'o', "easyfs",
		    "read regular file data");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close reopened file");

	fd = open("/alpha", O_WRONLY | O_TRUNC);
	TEST_ASSERT(fd >= 0, "easyfs", "truncate regular file");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close truncated file");

	fd = open("/alpha", O_RDONLY);
	TEST_ASSERT(fd >= 0, "easyfs", "reopen after truncate");
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 0, "easyfs", "truncated file reads EOF");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close truncated reopen");

	fd = open("/alpha", O_WRONLY | O_APPEND);
	TEST_ASSERT(fd >= 0, "easyfs", "open append");
	n = write(fd, "xy", 2);
	TEST_ASSERT(n == 2, "easyfs", "append write");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close append");

	fd = open("/alpha", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 2 && buf[0] == 'x' && buf[1] == 'y', "easyfs",
		    "append data after truncate");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close append read");

	int ret = mkdir("/dir", 0);
	TEST_ASSERT(ret == 0, "easyfs", "mkdir");

	fd = open("/dir/beta", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs", "create file in subdirectory");
	n = write(fd, "subfile", 7);
	TEST_ASSERT(n == 7, "easyfs", "write file in subdirectory");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close subdir file");

	fd = open("/dir/beta", O_RDONLY);
	TEST_ASSERT(fd >= 0, "easyfs", "reopen file in subdirectory");
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 7 && buf[0] == 's' && buf[6] == 'e', "easyfs",
		    "read file in subdirectory");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close subdir read");

	ret = unlink("/alpha");
	TEST_ASSERT(ret == 0, "easyfs", "unlink regular file");

	fd = open("/alpha", O_RDONLY);
	TEST_ASSERT(fd < 0, "easyfs", "unlinked file not reopenable");

	fd = open("/dir/beta", O_WRONLY | O_TRUNC);
	TEST_ASSERT(fd >= 0, "easyfs", "truncate subdirectory file");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close truncated subdir file");

	fd = open("/dir/beta", O_RDONLY);
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 0, "easyfs",
		    "subdirectory file reads EOF after truncate");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close subdir truncated read");

	fd = open("/gamma", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs", "create file after unlink");
	n = write(fd, "new", 3);
	TEST_ASSERT(n == 3, "easyfs", "write after unlink");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close new file");

	fd = open("/gamma", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 3 && buf[0] == 'n' && buf[2] == 'w', "easyfs",
		    "read file created after unlink");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close post-unlink read");

	fd = open("/alpha", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs", "recreate file after unlink same name");
	n = write(fd, "reborn", 6);
	TEST_ASSERT(n == 6, "easyfs", "write to recreated file");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close recreated file");

	fd = open("/alpha", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 6 && buf[0] == 'r' && buf[5] == 'n', "easyfs",
		    "recreated file has new data");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close recreated read");

	ret = mkdir("/dir/nest", 0);
	TEST_ASSERT(ret == 0, "easyfs", "nested mkdir");

	fd = open("/dir/nest/deep", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs", "create file in nested subdirectory");
	n = write(fd, "deep", 4);
	TEST_ASSERT(n == 4, "easyfs", "write file in nested directory");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close nested file");

	fd = open("/dir/nest/deep", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 4 && buf[0] == 'd' && buf[3] == 'p', "easyfs",
		    "read file in nested directory");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close nested read");

	fd = open("/dir/beta", O_WRONLY | O_APPEND);
	TEST_ASSERT(fd >= 0, "easyfs", "open append in subdirectory");
	n = write(fd, "ZZ", 2);
	TEST_ASSERT(n == 2, "easyfs", "append write in subdirectory");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close subdir append");

	fd = open("/dir/beta", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 2 && buf[0] == 'Z' && buf[1] == 'Z', "easyfs",
		    "append in subdirectory after truncate");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close subdir append read");

	fd = open("/dir/f2", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs", "create second file in subdirectory");
	n = write(fd, "two", 3);
	TEST_ASSERT(n == 3, "easyfs", "write second file");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close second file");

	fd = open("/dir/beta", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 2 && buf[0] == 'Z', "easyfs",
		    "first subdir file intact after second file creation");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close first subdir file");

	fd = open("/dir/f2", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 3 && buf[0] == 't' && buf[2] == 'o', "easyfs",
		    "second subdir file intact");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close second subdir read");

	fd = open("/trelvechar", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "easyfs", "create file with 12-char name");
	n = write(fd, "ok", 2);
	TEST_ASSERT(n == 2, "easyfs", "write long-name file");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close long-name file");

	fd = open("/trelvechar", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 2 && buf[0] == 'o', "easyfs", "read long-name file");
	TEST_ASSERT(close(fd) == 0, "easyfs", "close long-name read");

	ret = unlink("/trelvechar");
	TEST_ASSERT(ret == 0, "easyfs", "unlink long-name file");

	TEST_PASS("easyfs");
	shutdown();
}
