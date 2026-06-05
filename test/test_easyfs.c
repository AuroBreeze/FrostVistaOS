#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040
#define O_TRUNC 0x200
#define O_APPEND 0x400

static void test_file_create_read(void)
{
	TEST_START("test_file_create_read");

	int fd = open("/alpha", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_file_create_read", "create regular file");
	long n = write(fd, "hello", 5);
	TEST_ASSERT(n == 5, "test_file_create_read", "write regular file");
	close(fd);

	fd = open("/alpha", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_file_create_read",
		    "reopen regular file read-only");
	char buf[8] = {0};
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 5 && buf[0] == 'h' && buf[4] == 'o',
		    "test_file_create_read", "read regular file data");
	close(fd);
	TEST_PASS("test_file_create_read");
}

static void test_file_truncate(void)
{
	TEST_START("test_file_truncate");

	int fd = open("/alpha", O_WRONLY | O_TRUNC);
	TEST_ASSERT(fd >= 0, "test_file_truncate", "truncate regular file");
	close(fd);

	fd = open("/alpha", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_file_truncate", "reopen after truncate");
	char buf[8] = {0};
	long n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 0, "test_file_truncate", "truncated file reads EOF");
	close(fd);
	TEST_PASS("test_file_truncate");
}

static void test_file_append(void)
{
	TEST_START("test_file_append");

	int fd = open("/alpha", O_WRONLY | O_APPEND);
	TEST_ASSERT(fd >= 0, "test_file_append", "open append");
	long n = write(fd, "xy", 2);
	TEST_ASSERT(n == 2, "test_file_append", "append write");
	close(fd);

	fd = open("/alpha", O_RDONLY);
	char buf[8] = {0};
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 2 && buf[0] == 'x' && buf[1] == 'y',
		    "test_file_append", "append data after truncate");
	close(fd);
	TEST_PASS("test_file_append");
}

static void test_subdir_file(void)
{
	TEST_START("test_subdir_file");

	int ret = mkdir("/dir", 0);
	TEST_ASSERT(ret == 0, "test_subdir_file", "mkdir");

	int fd = open("/dir/beta", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_subdir_file", "create file in subdirectory");
	long n = write(fd, "subfile", 7);
	TEST_ASSERT(n == 7, "test_subdir_file", "write file in subdirectory");
	close(fd);

	fd = open("/dir/beta", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_subdir_file", "reopen file in subdirectory");
	char buf[16] = {0};
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 7 && buf[0] == 's' && buf[6] == 'e',
		    "test_subdir_file", "read file in subdirectory");
	close(fd);
	TEST_PASS("test_subdir_file");
}

static void test_unlink_recreate(void)
{
	TEST_START("test_unlink_recreate");

	int ret = unlink("/alpha");
	TEST_ASSERT(ret == 0, "test_unlink_recreate", "unlink regular file");

	int fd = open("/alpha", O_RDONLY);
	TEST_ASSERT(fd < 0, "test_unlink_recreate",
		    "unlinked file not reopenable");

	fd = open("/dir/beta", O_WRONLY | O_TRUNC);
	TEST_ASSERT(fd >= 0, "test_unlink_recreate",
		    "truncate subdirectory file");
	close(fd);

	fd = open("/dir/beta", O_RDONLY);
	char buf[8] = {0};
	long n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 0, "test_unlink_recreate",
		    "subdirectory file reads EOF after truncate");
	close(fd);

	fd = open("/gamma", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_unlink_recreate",
		    "create file after unlink");
	n = write(fd, "new", 3);
	TEST_ASSERT(n == 3, "test_unlink_recreate", "write after unlink");
	close(fd);

	fd = open("/gamma", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 3 && buf[0] == 'n' && buf[2] == 'w',
		    "test_unlink_recreate", "read file created after unlink");
	close(fd);

	fd = open("/alpha", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_unlink_recreate",
		    "recreate file after unlink same name");
	n = write(fd, "reborn", 6);
	TEST_ASSERT(n == 6, "test_unlink_recreate", "write to recreated file");
	close(fd);

	fd = open("/alpha", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 6 && buf[0] == 'r' && buf[5] == 'n',
		    "test_unlink_recreate", "recreated file has new data");
	close(fd);
	TEST_PASS("test_unlink_recreate");
}

static void test_nested_dir(void)
{
	TEST_START("test_nested_dir");

	int ret = mkdir("/dir/nest", 0);
	TEST_ASSERT(ret == 0, "test_nested_dir", "nested mkdir");

	int fd = open("/dir/nest/deep", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_nested_dir",
		    "create file in nested subdirectory");
	long n = write(fd, "deep", 4);
	TEST_ASSERT(n == 4, "test_nested_dir",
		    "write file in nested directory");
	close(fd);

	fd = open("/dir/nest/deep", O_RDONLY);
	char buf[8] = {0};
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 4 && buf[0] == 'd' && buf[3] == 'p', "test_nested_dir",
		    "read file in nested directory");
	close(fd);
	TEST_PASS("test_nested_dir");
}

static void test_multi_subdir(void)
{
	TEST_START("test_multi_subdir");

	int fd = open("/dir/beta", O_WRONLY | O_APPEND);
	TEST_ASSERT(fd >= 0, "test_multi_subdir",
		    "open append in subdirectory");
	long n = write(fd, "ZZ", 2);
	TEST_ASSERT(n == 2, "test_multi_subdir",
		    "append write in subdirectory");
	close(fd);

	fd = open("/dir/beta", O_RDONLY);
	char buf[16] = {0};
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 2 && buf[0] == 'Z' && buf[1] == 'Z',
		    "test_multi_subdir",
		    "append in subdirectory after truncate");
	close(fd);

	fd = open("/dir/f2", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_multi_subdir",
		    "create second file in subdirectory");
	n = write(fd, "two", 3);
	TEST_ASSERT(n == 3, "test_multi_subdir", "write second file");
	close(fd);

	fd = open("/dir/beta", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 2 && buf[0] == 'Z', "test_multi_subdir",
		    "first subdir file intact after second file creation");
	close(fd);

	fd = open("/dir/f2", O_RDONLY);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 3 && buf[0] == 't' && buf[2] == 'o',
		    "test_multi_subdir", "second subdir file intact");
	close(fd);
	TEST_PASS("test_multi_subdir");
}

static void test_long_name(void)
{
	TEST_START("test_long_name");

	int fd = open("/trelvechar", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_long_name", "create file with 12-char name");
	long n = write(fd, "ok", 2);
	TEST_ASSERT(n == 2, "test_long_name", "write long-name file");
	close(fd);

	fd = open("/trelvechar", O_RDONLY);
	char buf[4] = {0};
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 2 && buf[0] == 'o', "test_long_name",
		    "read long-name file");
	close(fd);

	int ret = unlink("/trelvechar");
	TEST_ASSERT(ret == 0, "test_long_name", "unlink long-name file");
	TEST_PASS("test_long_name");
}

void _start(void)
{
	TEST_START("easyfs");
	test_file_create_read();
	test_file_truncate();
	test_file_append();
	test_subdir_file();
	test_unlink_recreate();
	test_nested_dir();
	test_multi_subdir();
	test_long_name();
	TEST_PASS("easyfs");
	shutdown();
}
