#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR 0x002
#define O_CREAT 0x040
#define O_TRUNC 0x200

void _start(void)
{
	TEST_START("sys_open_flags");

	int fd = open("/dev/tty", O_RDONLY);
	printf("open(/dev/tty, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "sys_open_flags", "O_RDONLY should open tty");
	TEST_ASSERT(close(fd) == 0, "sys_open_flags",
		    "close O_RDONLY tty should succeed");

	fd = open("/dev/tty", O_WRONLY);
	printf("open(/dev/tty, O_WRONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "sys_open_flags", "O_WRONLY should open tty");
	TEST_ASSERT(close(fd) == 0, "sys_open_flags",
		    "close O_WRONLY tty should succeed");

	fd = open("/dev/tty", O_RDWR);
	printf("open(/dev/tty, O_RDWR) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "sys_open_flags", "O_RDWR should open tty");
	TEST_ASSERT(close(fd) == 0, "sys_open_flags",
		    "close O_RDWR tty should succeed");

	fd = open("/dev/tty", 3);
	printf("open(/dev/tty, invalid access mode 3) -> %d\n", fd);
	TEST_ASSERT(fd < 0, "sys_open_flags",
		    "invalid access mode should fail");

	fd = open("/dev/tty", O_RDONLY | O_TRUNC);
	printf("open(/dev/tty, O_RDONLY|O_TRUNC) -> %d\n", fd);
	TEST_ASSERT(fd < 0, "sys_open_flags",
		    "O_TRUNC with O_RDONLY should fail");

	fd = open("/definitely-missing-open-flags", O_WRONLY | O_CREAT);
	printf("open(missing, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd < 0, "sys_open_flags",
		    "O_CREAT is not implemented yet and should still fail");

	TEST_PASS("sys_open_flags");
	shutdown();
}
