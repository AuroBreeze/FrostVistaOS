#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040

static void test_ext4_create_fails(void)
{
	TEST_START("test_ext4_create_fails");
	int fd = open("/newfile", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd < 0, "test_ext4_create_fails",
		    "ext4: O_CREAT should fail on read-only fs");
	TEST_PASS("test_ext4_create_fails");
}

static void test_ext4_read_missing_fails(void)
{
	TEST_START("test_ext4_read_missing_fails");
	int fd = open("/newfile", O_RDONLY);
	TEST_ASSERT(fd < 0, "test_ext4_read_missing_fails",
		    "ext4: read missing file should fail");
	TEST_PASS("test_ext4_read_missing_fails");
}

void _start(void)
{
	TEST_START("backend");
	test_ext4_create_fails();
	test_ext4_read_missing_fails();
	TEST_PASS("backend");
	shutdown();
}
