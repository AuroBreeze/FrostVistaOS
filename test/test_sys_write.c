#include "user.h"
#include "libtest.h"

static void fill_buf(char *buf, int size)
{
	for (int i = 0; i < size - 1; i++) {
		buf[i] = 'a' + (i % 26);
	}
	buf[size - 1] = '\n';
}

static void test_zero_length_write(void)
{
	TEST_START("test_zero_length_write");
	char buf[4] = "abc";
	int ret = write(1, buf, 0);
	printf("write(stdout, buf, 0) -> %d\n", ret);
	TEST_ASSERT(ret == 0, "test_zero_length_write",
		    "zero-length write should return 0");
	TEST_PASS("test_zero_length_write");
}

static void test_invalid_fd(void)
{
	TEST_START("test_invalid_fd");
	char buf[4] = "abc";
	int ret = write(-1, buf, 1);
	printf("write(-1, buf, 1) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "test_invalid_fd", "negative fd should fail");
	TEST_PASS("test_invalid_fd");
}

static void test_null_buffer(void)
{
	TEST_START("test_null_buffer");
	int ret = write(1, (char *) 0, 1);
	printf("write(stdout, NULL, 1) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "test_null_buffer",
		    "null user buffer should fail when count is non-zero");
	ret = write(1, (char *) 0, 0);
	printf("write(stdout, NULL, 0) -> %d\n", ret);
	TEST_ASSERT(ret == 0, "test_null_buffer",
		    "null user buffer should be allowed for zero-length write");
	TEST_PASS("test_null_buffer");
}

static void test_copy_boundaries(void)
{
	TEST_START("test_copy_boundaries");
	char small[1] = {'x'};
	char chunk255[255];
	char chunk256[256];
	char chunk257[257];
	fill_buf(chunk255, sizeof(chunk255));
	fill_buf(chunk256, sizeof(chunk256));
	fill_buf(chunk257, sizeof(chunk257));
	TEST_ASSERT(write(1, small, sizeof(small)) == (long) sizeof(small),
		    "test_copy_boundaries", "1-byte write should succeed");
	TEST_ASSERT(write(1, chunk255, sizeof(chunk255)) ==
			(long) sizeof(chunk255),
		    "test_copy_boundaries", "255-byte write should succeed");
	TEST_ASSERT(write(1, chunk256, sizeof(chunk256)) ==
			(long) sizeof(chunk256),
		    "test_copy_boundaries", "256-byte write should succeed");
	TEST_ASSERT(write(1, chunk257, sizeof(chunk257)) ==
			(long) sizeof(chunk257),
		    "test_copy_boundaries", "257-byte write should succeed");
	TEST_PASS("test_copy_boundaries");
}

static void test_normal_write(void)
{
	TEST_START("test_normal_write");
	char buf[400];
	for (int i = 0; i < 399; i++)
		buf[i] = 'A' + (i % 26);
	buf[399] = '\n';
	int ret = write(1, buf, sizeof(buf));
	printf("write(stdout, 400-byte buf) -> %d\n", ret);
	TEST_ASSERT(ret == sizeof(buf), "test_normal_write",
		    "normal write should return full count");
	TEST_PASS("test_normal_write");
}

void _start()
{
	TEST_START("sys_write");
	test_zero_length_write();
	test_invalid_fd();
	test_null_buffer();
	test_copy_boundaries();
	test_normal_write();
	TEST_PASS("sys_write");
	shutdown();
}
