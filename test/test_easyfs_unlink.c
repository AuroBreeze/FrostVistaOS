#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040

void _start(void)
{
	TEST_START("easyfs_unlink");

	int fd = open("/gone", O_WRONLY | O_CREAT);
	printf("open(/gone, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_unlink",
		    "O_CREAT should create file to unlink");
	long n = write(fd, "bye", 3);
	printf("write(/gone, bye) -> %d\n", (int) n);
	TEST_ASSERT(n == 3, "easyfs_unlink",
		    "write to unlink target should succeed");
	TEST_ASSERT(close(fd) == 0, "easyfs_unlink",
		    "close unlink target should succeed");

	int ret = unlink("/gone");
	printf("unlink(/gone) -> %d\n", ret);
	TEST_ASSERT(ret == 0, "easyfs_unlink",
		    "unlink regular file should succeed");

	fd = open("/gone", O_RDONLY);
	printf("open(/gone after unlink, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd < 0, "easyfs_unlink",
		    "unlinked file should not be reopenable");

	fd = open("/survive", O_WRONLY | O_CREAT);
	printf("open(/survive, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_unlink",
		    "O_CREAT should create file after unlink");
	n = write(fd, "ok", 2);
	printf("write(/survive, ok) -> %d\n", (int) n);
	TEST_ASSERT(n == 2, "easyfs_unlink",
		    "write after unlink should succeed");
	TEST_ASSERT(close(fd) == 0, "easyfs_unlink",
		    "close survived file should succeed");

	fd = open("/survive", O_RDONLY);
	printf("open(/survive, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_unlink",
		    "file created after unlink should reopen read-only");
	char buf[4] = {0};
	n = read(fd, buf, sizeof(buf));
	printf("read(/survive) -> %d, '%s'\n", (int) n, buf);
	TEST_ASSERT(n == 2, "easyfs_unlink",
		    "reopened file after unlink should read own data");
	TEST_ASSERT(buf[0] == 'o' && buf[1] == 'k' && buf[2] == '\0',
		    "easyfs_unlink",
		    "reopened file after unlink should have correct data");
	TEST_ASSERT(close(fd) == 0, "easyfs_unlink",
		    "close survived reopen should succeed");

	ret = unlink("/nope");
	printf("unlink(/nope) -> %d\n", ret);
	TEST_ASSERT(ret < 0, "easyfs_unlink",
		    "unlink missing file should fail");

	TEST_PASS("easyfs_unlink");
	shutdown();
}
