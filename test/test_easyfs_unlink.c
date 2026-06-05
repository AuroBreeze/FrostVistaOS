#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040

static void test_unlink_regular_file(void)
{
	TEST_START("test_unlink_regular_file");
	int fd = open("/gone", O_WRONLY | O_CREAT);
	printf("open(/gone, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_unlink_regular_file",
		    "O_CREAT should create file to unlink");
	long n = write(fd, "bye", 3);
	printf("write(/gone, bye) -> %d\n", (int) n);
	TEST_ASSERT(n == 3, "test_unlink_regular_file",
		    "write to unlink target should succeed");
	close(fd);

	int ret = unlink("/gone");
	printf("unlink(/gone) -> %d\n", ret);
	TEST_ASSERT(ret == 0, "test_unlink_regular_file",
		    "unlink regular file should succeed");

	fd = open("/gone", O_RDONLY);
	printf("open(/gone after unlink, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd < 0, "test_unlink_regular_file",
		    "unlinked file should not be reopenable");
	TEST_PASS("test_unlink_regular_file");
}

static void test_create_after_unlink(void)
{
	TEST_START("test_create_after_unlink");
	int fd = open("/survive", O_WRONLY | O_CREAT);
	printf("open(/survive, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_create_after_unlink",
		    "O_CREAT should create file after unlink");
	long n = write(fd, "ok", 2);
	printf("write(/survive, ok) -> %d\n", (int) n);
	TEST_ASSERT(n == 2, "test_create_after_unlink",
		    "write after unlink should succeed");
	close(fd);

	fd = open("/survive", O_RDONLY);
	printf("open(/survive, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_create_after_unlink",
		    "file created after unlink should reopen read-only");
	char buf[4] = {0};
	n = read(fd, buf, sizeof(buf));
	printf("read(/survive) -> %d, '%s'\n", (int) n, buf);
	TEST_ASSERT(n == 2, "test_create_after_unlink",
		    "reopened file after unlink should read own data");
	TEST_ASSERT(buf[0] == 'o' && buf[1] == 'k', "test_create_after_unlink",
		    "reopened file after unlink should have correct data");
	close(fd);
	TEST_PASS("test_create_after_unlink");
}

static void test_unlink_missing_fails(void)
{
	TEST_START("test_unlink_missing_fails");
	int ret = unlink("/nope");
	printf("unlink(/nope) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "test_unlink_missing_fails",
		    "unlink missing file should fail");
	TEST_PASS("test_unlink_missing_fails");
}

void _start(void)
{
	TEST_START("easyfs_unlink");
	test_unlink_regular_file();
	test_create_after_unlink();
	test_unlink_missing_fails();
	TEST_PASS("easyfs_unlink");
	shutdown();
}
