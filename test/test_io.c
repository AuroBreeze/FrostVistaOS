// test sys_close and sys_dup
#include "user.h"
#include "libtest.h"

void _start()
{
	TEST_START("io");
	printf("--- Starting Dup & Close Test ---\n");

	int fd1 = open("/dev/tty", 2); // O_RDWR = 2
	printf("open(/dev/tty, O_RDWR) -> %d\n", fd1);
	if (fd1 < 0) {
		printf("Test failed: Could not open /dev/tty\n");
		TEST_FAIL("io");
	}
	int ret = close(-1);
	printf("close(-1) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "io", "close(-1) should fail");
	ret = dup(-1);
	printf("dup(-1) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "io", "dup(-1) should fail");
	char read_buf[4];
	ret = read(-1, read_buf, sizeof(read_buf));
	printf("read(-1, buf, 4) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "io", "read(-1) should fail");
	ret = read(fd1, read_buf, 0);
	printf("read(fd1, buf, 0) -> %d\n", ret);
	TEST_ASSERT(ret == 0, "io", "zero-length read should return 0");

	int fd2 = dup(fd1);
	printf("dup(fd1) -> %d\n", fd2);
	if (fd2 < 0) {
		printf("Test failed: dup(fd1) failed\n");
		TEST_FAIL("io");
	}
	printf("Success: Opened fd %d, duped to fd %d\n", fd1, fd2);

	// Verify that both FDs are working
	write(fd1, "Message from fd1\n", 17);
	write(fd2, "Message from fd2 (duped)\n", 25);

	// Verify reference counting: Close fd1; fd2 should still be writable.
	ret = close(fd1);
	printf("close(fd1) -> %d\n", ret);
	ret = close(fd1);
	printf("close(fd1) again -> %d\n", ret);
	TEST_ASSERT(ret < 0, "io", "double close should fail");
	ret = write(fd1, "closed fd\n", 10);
	printf("write(closed fd1) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "io", "write on closed fd should fail");
	ret = read(fd1, read_buf, 1);
	printf("read(closed fd1) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "io", "read on closed fd should fail");
	ret = dup(fd1);
	printf("dup(closed fd1) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "io", "dup on closed fd should fail");
	printf("Closed fd %d, trying to write to fd %d...\n", fd1, fd2);
	if (write(fd2, "Still working after close!\n", 27) < 0) {
		printf("Error: duped fd failed after original closed\n");
	} else {
		printf("Success: Duped fd is persistent.\n");
	}

	// Verify the FD recycling logic (FrostVistaOS should assign the lowest
	// available ID)
	ret = close(fd2);
	printf("close(fd2) -> %d\n", ret);
	ret = close(fd2);
	printf("close(fd2) again -> %d\n", ret);
	TEST_ASSERT(ret < 0, "io", "closing duped fd twice should fail");
	int fd3 = open("/dev/tty", 2);
	printf("After closing all, new open returned fd: %d\n", fd3);

	if (fd3 == fd1 || fd3 == fd2) {
		printf("Success: FD %d was correctly recycled.\n", fd3);
	} else {
		printf("Note: FD recycled to %d (Expected %d or %d)\n", fd3,
		       fd1, fd2);
	}
	TEST_ASSERT(close(fd3) == 0, "io", "close recycled fd should succeed");
	TEST_ASSERT(close(fd3) < 0, "io",
		    "double close recycled fd should fail");

	TEST_PASS("io");
	printf("--- Dup & Close Test Finished ---\n");
	shutdown();
}
