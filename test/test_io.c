#include "user.h"
#include "libtest.h"

static void test_close_invalid_fd(void)
{
	TEST_START("test_close_invalid_fd");
	int ret = close(-1);
	printf("close(-1) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "test_close_invalid_fd", "close(-1) should fail");
	TEST_PASS("test_close_invalid_fd");
}

static void test_dup_invalid_fd(void)
{
	TEST_START("test_dup_invalid_fd");
	int ret = dup(-1);
	printf("dup(-1) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "test_dup_invalid_fd", "dup(-1) should fail");
	TEST_PASS("test_dup_invalid_fd");
}

static void test_read_invalid_fd(void)
{
	TEST_START("test_read_invalid_fd");
	char read_buf[4];
	int ret = read(-1, read_buf, sizeof(read_buf));
	printf("read(-1, buf, 4) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "test_read_invalid_fd", "read(-1) should fail");
	TEST_PASS("test_read_invalid_fd");
}

static void test_zero_length_read(void)
{
	TEST_START("test_zero_length_read");
	int fd = open("/dev/tty", 2);
	TEST_ASSERT(fd >= 0, "test_zero_length_read",
		    "open /dev/tty should succeed");
	char read_buf[4];
	int ret = read(fd, read_buf, 0);
	printf("read(fd, buf, 0) -> %d\n", ret);
	TEST_ASSERT(ret == 0, "test_zero_length_read",
		    "zero-length read should return 0");
	TEST_ASSERT(close(fd) == 0, "test_zero_length_read",
		    "close fd should succeed");
	TEST_PASS("test_zero_length_read");
}

static void test_dup_persistence(void)
{
	TEST_START("test_dup_persistence");
	int fd1 = open("/dev/tty", 2);
	TEST_ASSERT(fd1 >= 0, "test_dup_persistence",
		    "open /dev/tty should succeed");

	int fd2 = dup(fd1);
	printf("dup(fd1) -> %d\n", fd2);
	TEST_ASSERT(fd2 >= 0, "test_dup_persistence",
		    "dup(fd1) should succeed");

	write(fd1, "Message from fd1\n", 17);
	write(fd2, "Message from fd2 (duped)\n", 25);

	TEST_ASSERT(close(fd1) == 0, "test_dup_persistence",
		    "close(fd1) should succeed");
	TEST_ASSERT(close(fd1) < 0, "test_dup_persistence",
		    "double close should fail");
	TEST_ASSERT(write(fd1, "closed fd\n", 10) < 0, "test_dup_persistence",
		    "write on closed fd should fail");

	if (write(fd2, "Still working after close!\n", 27) >= 0) {
		printf("Success: Duped fd is persistent.\n");
	} else {
		printf("Error: duped fd failed after original closed\n");
		TEST_FAIL("test_dup_persistence");
	}

	TEST_ASSERT(close(fd2) == 0, "test_dup_persistence",
		    "close(fd2) should succeed");
	TEST_ASSERT(close(fd2) < 0, "test_dup_persistence",
		    "closing duped fd twice should fail");
	TEST_PASS("test_dup_persistence");
}

static void test_fd_recycle(void)
{
	TEST_START("test_fd_recycle");
	int fd1 = open("/dev/tty", 2);
	TEST_ASSERT(fd1 >= 0, "test_fd_recycle", "first open should succeed");
	int fd2 = dup(fd1);
	TEST_ASSERT(fd2 >= 0, "test_fd_recycle", "dup should succeed");

	TEST_ASSERT(close(fd1) == 0, "test_fd_recycle",
		    "close first fd should succeed");
	TEST_ASSERT(close(fd2) == 0, "test_fd_recycle",
		    "close dup fd should succeed");

	int fd3 = open("/dev/tty", 2);
	printf("After closing all, new open returned fd: %d\n", fd3);
	if (fd3 == fd1 || fd3 == fd2) {
		printf("Success: FD %d was correctly recycled.\n", fd3);
	} else {
		printf("Note: FD recycled to %d (Expected %d or %d)\n", fd3,
		       fd1, fd2);
	}
	TEST_ASSERT(close(fd3) == 0, "test_fd_recycle",
		    "close recycled fd should succeed");
	TEST_ASSERT(close(fd3) < 0, "test_fd_recycle",
		    "double close recycled fd should fail");
	TEST_PASS("test_fd_recycle");
}

void _start()
{
	TEST_START("io");
	printf("--- Starting Dup & Close Test ---\n");
	test_close_invalid_fd();
	test_dup_invalid_fd();
	test_read_invalid_fd();
	test_zero_length_read();
	test_dup_persistence();
	test_fd_recycle();
	TEST_PASS("io");
	printf("--- Dup & Close Test Finished ---\n");
	shutdown();
}
