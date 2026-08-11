#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040
#define O_TRUNC 0x200

static void test_tmpfs_basic(void)
{
	TEST_START("test_tmpfs_basic");

	int fd = open("/tmp/ut1", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_tmpfs_basic", "create /tmp/ut1");
	TEST_ASSERT(write(fd, "hello", 5) == 5, "test_tmpfs_basic", "write");
	close(fd);

	fd = open("/tmp/ut1", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_tmpfs_basic", "reopen read-only");
	char buf[8] = {0};
	long n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 5 && buf[0] == 'h' && buf[4] == 'o',
		    "test_tmpfs_basic", "read back");
	close(fd);
	TEST_PASS("test_tmpfs_basic");
}

static void test_tmpfs_mkdir_unlink(void)
{
	TEST_START("test_tmpfs_mkdir_unlink");

	TEST_ASSERT(mkdir("/tmp/udir", 0) == 0, "test_tmpfs_mkdir_unlink",
		    "mkdir /tmp/udir");

	int fd = open("/tmp/udir/uf", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_tmpfs_mkdir_unlink", "create inside dir");
	close(fd);

	TEST_ASSERT(unlink("/tmp/udir/uf") == 0, "test_tmpfs_mkdir_unlink",
		    "unlink file");
	TEST_ASSERT(open("/tmp/udir/uf", O_RDONLY) < 0,
		    "test_tmpfs_mkdir_unlink", "file should be gone");
	TEST_PASS("test_tmpfs_mkdir_unlink");
}

static void test_tmpfs_truncate(void)
{
	TEST_START("test_tmpfs_truncate");

	int fd = open("/tmp/utr", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_tmpfs_truncate", "create /tmp/utr");
	char data[4096 + 100];
	for (int i = 0; i < (int) sizeof(data); i++)
		data[i] = 'a' + (i % 26);
	TEST_ASSERT(write(fd, data, sizeof(data)) == (long) sizeof(data),
		    "test_tmpfs_truncate", "write multi-page");
	close(fd);

	fd = open("/tmp/utr", O_WRONLY | O_TRUNC);
	TEST_ASSERT(fd >= 0, "test_tmpfs_truncate", "open with O_TRUNC");
	close(fd);

	fd = open("/tmp/utr", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_tmpfs_truncate", "reopen after truncate");
	char buf[8] = {0};
	TEST_ASSERT(read(fd, buf, sizeof(buf)) == 0, "test_tmpfs_truncate",
		    "truncated file reads EOF");
	close(fd);
	TEST_PASS("test_tmpfs_truncate");
}

void _start(void)
{
	test_tmpfs_basic();
	test_tmpfs_mkdir_unlink();
	test_tmpfs_truncate();
	shutdown();
}
