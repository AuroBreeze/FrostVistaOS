#include "user.h"
#include "libtest.h"

#define O_WRONLY 0x001

void _start()
{
	TEST_START("vfs");
	printf("VFS Test: Attempting to open /dev/tty...\n");
	int ret = open("/definitely-not-a-real-file", O_WRONLY);
	printf("open(missing path, O_WRONLY) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "vfs",
		    "opening a missing absolute path should fail");
	ret = open("", O_WRONLY);
	printf("open(empty path, O_WRONLY) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "vfs", "opening an empty path should fail");

	int ro_fd = open("/dev/tty", 0); // O_RDONLY
	printf("open(/dev/tty, O_RDONLY) -> %d\n", ro_fd);
	if (ro_fd >= 0) {
		ret = write(ro_fd, "readonly\n", 9);
		printf("write(read-only fd) -> %d\n", ret);
		TEST_ASSERT(ret < 0, "vfs",
			    "write through read-only fd should fail");
		ret = close(ro_fd);
		printf("close(read-only fd) -> %d\n", ret);
		TEST_ASSERT(ret == 0, "vfs",
			    "closing read-only fd should succeed");
	}

	// 1. Test by turning on the device
	int fd = open("/dev/tty", O_WRONLY);
	printf("open(/dev/tty, O_WRONLY) -> %d\n", fd);

	if (fd < 0) {
		printf("VFS Test: Open /dev/tty failed! (fd=%d)\n", fd);
		TEST_FAIL("vfs");
	}

	printf("VFS Test: Open success, got fd=%d\n", fd);

	// 2. Test writing data via FD
	char *msg = ">>> [VFS Success]: Data written via VFS to UART! <<<\n";
	long bytes = write(fd, msg, strlen(msg));

	if (bytes > 0) {
		printf("VFS Test: Successfully wrote %d bytes.\n", (int) bytes);
	} else {
		printf("VFS Test: Write failed!\n");
		TEST_FAIL("vfs");
	}
	TEST_ASSERT(close(fd) == 0, "vfs", "close /dev/tty should succeed");
	TEST_ASSERT(close(fd) < 0, "vfs", "double close /dev/tty should fail");
	TEST_PASS("vfs");

	shutdown();
}
