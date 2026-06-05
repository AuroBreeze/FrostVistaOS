#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040

static void test_mkdir_basic(void)
{
	TEST_START("test_mkdir_basic");
	int ret = mkdir("/sub", 0);
	printf("mkdir(/sub) -> %d\n", ret);
	TEST_ASSERT(ret == 0, "test_mkdir_basic",
		    "mkdir should create an empty directory");

	int fd = open("/sub/file", O_WRONLY | O_CREAT);
	printf("open(/sub/file, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_mkdir_basic",
		    "should create a file inside the new directory");
	long n = write(fd, "ok", 2);
	printf("write(/sub/file, ok) -> %d\n", (int) n);
	TEST_ASSERT(n == 2, "test_mkdir_basic",
		    "should write to file inside subdirectory");
	close(fd);

	fd = open("/sub/file", O_RDONLY);
	printf("open(/sub/file, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_mkdir_basic",
		    "should reopen file inside subdirectory");
	char buf[4] = {0};
	n = read(fd, buf, sizeof(buf));
	printf("read(/sub/file) -> %d, '%s'\n", (int) n, buf);
	TEST_ASSERT(n == 2 && buf[0] == 'o' && buf[1] == 'k',
		    "test_mkdir_basic",
		    "should read correct data from file in subdirectory");
	close(fd);
	TEST_PASS("test_mkdir_basic");
}

static void test_mkdir_duplicate_fails(void)
{
	TEST_START("test_mkdir_duplicate_fails");
	int ret = mkdir("/sub", 0);
	printf("mkdir(/sub again) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "test_mkdir_duplicate_fails",
		    "duplicate mkdir should fail");
	TEST_PASS("test_mkdir_duplicate_fails");
}

static void test_mkdir_empty_path_fails(void)
{
	TEST_START("test_mkdir_empty_path_fails");
	int ret = mkdir("", 0);
	printf("mkdir(\"\") -> %d\n", ret);
	TEST_ASSERT(ret < 0, "test_mkdir_empty_path_fails",
		    "empty path mkdir should fail");
	TEST_PASS("test_mkdir_empty_path_fails");
}

void _start(void)
{
	TEST_START("easyfs_mkdir");
	test_mkdir_basic();
	test_mkdir_duplicate_fails();
	test_mkdir_empty_path_fails();
	TEST_PASS("easyfs_mkdir");
	shutdown();
}
