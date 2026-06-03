#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040

void _start(void)
{
	TEST_START("easyfs_mkdir");

	int ret = mkdir("/sub", 0);
	printf("mkdir(/sub) -> %d\n", ret);
	TEST_ASSERT(ret == 0, "easyfs_mkdir",
		    "mkdir should create an empty directory");

	int fd = open("/sub/file", O_WRONLY | O_CREAT);
	printf("open(/sub/file, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_mkdir",
		    "should create a file inside the new directory");
	long n = write(fd, "ok", 2);
	printf("write(/sub/file, ok) -> %d\n", (int) n);
	TEST_ASSERT(n == 2, "easyfs_mkdir",
		    "should write to file inside subdirectory");
	TEST_ASSERT(close(fd) == 0, "easyfs_mkdir",
		    "close subdir file should succeed");

	fd = open("/sub/file", O_RDONLY);
	printf("open(/sub/file, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_mkdir",
		    "should reopen file inside subdirectory");
	char buf[4] = {0};
	n = read(fd, buf, sizeof(buf));
	printf("read(/sub/file) -> %d, '%s'\n", (int) n, buf);
	TEST_ASSERT(n == 2 && buf[0] == 'o' && buf[1] == 'k', "easyfs_mkdir",
		    "should read correct data from file in subdirectory");
	TEST_ASSERT(close(fd) == 0, "easyfs_mkdir",
		    "close subdir reopen should succeed");

	ret = mkdir("/sub", 0);
	printf("mkdir(/sub again) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "easyfs_mkdir", "duplicate mkdir should fail");

	ret = mkdir("", 0);
	printf("mkdir(\"\") -> %d\n", ret);
	TEST_ASSERT(ret < 0, "easyfs_mkdir", "empty path mkdir should fail");

	TEST_PASS("easyfs_mkdir");
	shutdown();
}
