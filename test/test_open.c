#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR 0x002
#define O_CREAT 0x040
#define O_TRUNC 0x200
#define O_APPEND 0x400

void _start(void)
{
	TEST_START("open");

	int fd = open("/dev/tty", O_RDONLY);
	printf("open(/dev/tty, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_RDONLY should open tty");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close O_RDONLY tty should succeed");

	fd = open("/dev/tty", O_WRONLY);
	printf("open(/dev/tty, O_WRONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_WRONLY should open tty");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close O_WRONLY tty should succeed");

	fd = open("/dev/tty", O_RDWR);
	printf("open(/dev/tty, O_RDWR) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_RDWR should open tty");
	TEST_ASSERT(close(fd) == 0, "open", "close O_RDWR tty should succeed");

	fd = open("/dev/tty", 3);
	printf("open(/dev/tty, invalid access mode 3) -> %d\n", fd);
	TEST_ASSERT(fd < 0, "open", "invalid access mode should fail");

	fd = open("/dev/tty", O_RDONLY | O_TRUNC);
	printf("open(/dev/tty, O_RDONLY|O_TRUNC) -> %d\n", fd);
	TEST_ASSERT(fd < 0, "open", "O_TRUNC with O_RDONLY should fail");

	fd = open("/oflags", O_WRONLY | O_CREAT);
	printf("open(/oflags, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_CREAT should create a missing file");
	char data[] = "hello";
	long n = write(fd, data, 5);
	printf("write(/oflags, hello) -> %d\n", (int) n);
	TEST_ASSERT(n == 5, "open", "write created file should succeed");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close written file should succeed");

	fd = open("/oflags", O_RDONLY);
	printf("open(/oflags, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "created file should reopen read-only");
	char buf[8] = {0};
	n = read(fd, buf, sizeof(buf));
	printf("read(/oflags) -> %d, '%s'\n", (int) n, buf);
	TEST_ASSERT(n == 5, "open", "reopened file should read written size");
	TEST_ASSERT(buf[0] == 'h' && buf[1] == 'e' && buf[2] == 'l' &&
			buf[3] == 'l' && buf[4] == 'o' && buf[5] == '\0',
		    "open", "reopened file should read written data");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close reopened file should succeed");

	fd = open("/oflags", O_WRONLY | O_TRUNC);
	printf("open(/oflags, O_WRONLY|O_TRUNC) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_TRUNC should open existing file");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close truncated file should succeed");

	fd = open("/oflags", O_RDONLY);
	printf("open(/oflags after truncate, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "truncated file should reopen read-only");
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	printf("read(/oflags after truncate) -> %d\n", (int) n);
	TEST_ASSERT(n == 0, "open", "truncated file should read EOF");
	TEST_ASSERT(buf[0] == '\0', "open",
		    "truncated file should not expose stale data");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close truncated reopen should succeed");

	fd = open("/append", O_WRONLY | O_CREAT);
	printf("open(/append, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_CREAT should create append test file");
	n = write(fd, "he", 2);
	printf("write(/append, he) -> %d\n", (int) n);
	TEST_ASSERT(n == 2, "open", "initial append test write should succeed");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close append test file should succeed");

	fd = open("/append", O_WRONLY | O_APPEND);
	printf("open(/append, O_WRONLY|O_APPEND) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_APPEND should open existing file");
	n = write(fd, "llo", 3);
	printf("write(/append, llo) -> %d\n", (int) n);
	TEST_ASSERT(n == 3, "open", "append write should succeed");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close append write file should succeed");

	fd = open("/append", O_RDONLY);
	printf("open(/append, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open",
		    "append test file should reopen read-only");
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	printf("read(/append) -> %d, '%s'\n", (int) n, buf);
	TEST_ASSERT(n == 5, "open", "append result should read full size");
	TEST_ASSERT(buf[0] == 'h' && buf[1] == 'e' && buf[2] == 'l' &&
			buf[3] == 'l' && buf[4] == 'o' && buf[5] == '\0',
		    "open", "append result should preserve existing data");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close append read file should succeed");

	TEST_PASS("open");
	shutdown();
}
