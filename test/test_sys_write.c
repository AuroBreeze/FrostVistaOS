#include "user.h"
#include "libtest.h"

static void fill_buf(char *buf, int size)
{
	for (int i = 0; i < size - 1; i++) {
		buf[i] = 'a' + (i % 26); // generate a-z
	}
	buf[size - 1] = '\n';
}

int _start()
{
	TEST_START("sys_write");
	int buf_size = 400;

	// testing with 400 bytes
	char buf[400];

	for (int i = 0; i < buf_size - 1; i++) {
		buf[i] = 'A' + (i % 26); // generate A-Z
	}
	buf[buf_size - 1] = '\n';

	printf("=========================================\n");
	printf("Starting sys_write test with %d bytes...\n", buf_size);
	printf("=========================================\n");

	// A zero-length write should be a successful no-op.
	int ret = write(1, buf, 0);
	printf("write(stdout, buf, 0) -> %d\n", ret);
	TEST_ASSERT(ret == 0, "sys_write", "zero-length write should return 0");

	// Invalid descriptors and invalid user pointers should fail cleanly.
	ret = write(-1, buf, 1);
	printf("write(-1, buf, 1) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "sys_write", "negative fd should fail");
	ret = write(1, (char *) 0, 1);
	printf("write(stdout, NULL, 1) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "sys_write",
		    "null user buffer should fail when count is non-zero");
	ret = write(1, (char *) 0, 0);
	printf("write(stdout, NULL, 0) -> %d\n", ret);
	TEST_ASSERT(ret == 0, "sys_write",
		    "null user buffer should be allowed for zero-length write");

	// Exercise the kernel's internal 256-byte copy buffer boundaries.
	char small[1] = {'x'};
	char chunk255[255];
	char chunk256[256];
	char chunk257[257];
	fill_buf(chunk255, sizeof(chunk255));
	fill_buf(chunk256, sizeof(chunk256));
	fill_buf(chunk257, sizeof(chunk257));
	TEST_ASSERT(write(1, small, sizeof(small)) == (long) sizeof(small),
		    "sys_write", "1-byte write should succeed");
	TEST_ASSERT(write(1, chunk255, sizeof(chunk255)) ==
			(long) sizeof(chunk255),
		    "sys_write", "255-byte write should succeed");
	TEST_ASSERT(write(1, chunk256, sizeof(chunk256)) ==
			(long) sizeof(chunk256),
		    "sys_write", "256-byte write should succeed");
	TEST_ASSERT(write(1, chunk257, sizeof(chunk257)) ==
			(long) sizeof(chunk257),
		    "sys_write", "257-byte write should succeed");

	ret = write(1, buf, buf_size);

	// 打印测试结果
	printf("\n\n");
	printf("=========================================\n");
	printf("              Test Results               \n");
	printf("=========================================\n");
	printf("Requested to write : %d bytes\n", buf_size);
	printf("Return value       : %d bytes\n", ret);

	if (ret < buf_size) {
		printf("Status: WARNING! Write was truncated by the kernel.\n");
		printf("If return value is %d, the kernel only processed "
		       "partial data.\n",
		       ret);
		TEST_FAIL("sys_write");
	} else {
		printf("Status: SUCCESS! All bytes were written.\n");
		TEST_PASS("sys_write");
	}

	shutdown();
}
