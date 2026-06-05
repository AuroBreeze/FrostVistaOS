#include "user.h"
#include "libtest.h"

#define O_WRONLY 0x001

static void test_open_missing_path(void)
{
	TEST_START("test_open_missing_path");
	int ret = open("/definitely-not-a-real-file", O_WRONLY);
	printf("open(missing path, O_WRONLY) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "test_open_missing_path",
		    "opening a missing absolute path should fail");
	TEST_PASS("test_open_missing_path");
}

static void test_open_empty_path(void)
{
	TEST_START("test_open_empty_path");
	int ret = open("", O_WRONLY);
	printf("open(empty path, O_WRONLY) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "test_open_empty_path",
		    "opening an empty path should fail");
	TEST_PASS("test_open_empty_path");
}

static void test_readonly_write_fails(void)
{
	TEST_START("test_readonly_write_fails");
	int ro_fd = open("/dev/tty", 0);
	printf("open(/dev/tty, O_RDONLY) -> %d\n", ro_fd);
	if (ro_fd >= 0) {
		int ret = write(ro_fd, "readonly\n", 9);
		printf("write(read-only fd) -> %d\n", ret);
		TEST_ASSERT(ret < 0, "test_readonly_write_fails",
			    "write through read-only fd should fail");
		ret = close(ro_fd);
		printf("close(read-only fd) -> %d\n", ret);
		TEST_ASSERT(ret == 0, "test_readonly_write_fails",
			    "closing read-only fd should succeed");
	}
	TEST_PASS("test_readonly_write_fails");
}

static void test_vfs_normal_write(void)
{
	TEST_START("test_vfs_normal_write");
	int fd = open("/dev/tty", O_WRONLY);
	printf("open(/dev/tty, O_WRONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_vfs_normal_write",
		    "open /dev/tty should succeed");

	char *msg = ">>> [VFS Success]: Data written via VFS to UART! <<<\n";
	long bytes = write(fd, msg, strlen(msg));
	TEST_ASSERT(bytes > 0, "test_vfs_normal_write",
		    "write to /dev/tty should succeed");
	printf("VFS Test: Successfully wrote %d bytes.\n", (int) bytes);
	TEST_ASSERT(close(fd) == 0, "test_vfs_normal_write",
		    "close /dev/tty should succeed");
	TEST_PASS("test_vfs_normal_write");
}

static void test_double_close(void)
{
	TEST_START("test_double_close");
	int fd = open("/dev/tty", O_WRONLY);
	TEST_ASSERT(fd >= 0, "test_double_close",
		    "open /dev/tty should succeed");
	TEST_ASSERT(close(fd) == 0, "test_double_close",
		    "first close should succeed");
	TEST_ASSERT(close(fd) < 0, "test_double_close",
		    "double close should fail");
	TEST_PASS("test_double_close");
}

void _start()
{
	TEST_START("vfs");
	printf("VFS Test: Attempting to open /dev/tty...\n");
	test_open_missing_path();
	test_open_empty_path();
	test_readonly_write_fails();
	test_vfs_normal_write();
	test_double_close();
	TEST_PASS("vfs");
	shutdown();
}
