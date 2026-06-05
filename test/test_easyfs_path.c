#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040

static void test_empty_path_fails(void)
{
	TEST_START("test_empty_path_fails");
	int fd = open("", O_RDONLY);
	TEST_ASSERT(fd < 0, "test_empty_path_fails",
		    "empty path open should fail");
	TEST_PASS("test_empty_path_fails");
}

static void test_create_under_missing_parent_fails(void)
{
	TEST_START("test_create_under_missing_parent_fails");
	int fd = open("/nonexist/file", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd < 0, "test_create_under_missing_parent_fails",
		    "create under missing parent should fail");
	TEST_PASS("test_create_under_missing_parent_fails");
}

static void test_create_under_file_fails(void)
{
	TEST_START("test_create_under_file_fails");
	int fd = open("/alpha", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_create_under_file_fails",
		    "create regular file /alpha");
	write(fd, "x", 1);
	close(fd);

	fd = open("/alpha/sub", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd < 0, "test_create_under_file_fails",
		    "create under non-directory should fail");

	unlink("/alpha");
	TEST_PASS("test_create_under_file_fails");
}

static void test_name_dirsiz_boundary(void)
{
	TEST_START("test_name_dirsiz_boundary");
	int fd = open("/abcdefghijk1234", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd < 0, "test_name_dirsiz_boundary",
		    "name at DIRSIZ boundary should fail");

	fd = open("/abcdefghijk12", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_name_dirsiz_boundary",
		    "name one below DIRSIZ should succeed");
	write(fd, "ok", 2);
	close(fd);

	fd = open("/abcdefghijk12", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_name_dirsiz_boundary",
		    "reopen DIRSIZ-1 name file");
	char buf[4] = {0};
	long n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 2 && buf[0] == 'o', "test_name_dirsiz_boundary",
		    "read DIRSIZ-1 name file");
	close(fd);
	TEST_PASS("test_name_dirsiz_boundary");
}

void _start(void)
{
	TEST_START("easyfs_path");
	test_empty_path_fails();
	test_create_under_missing_parent_fails();
	test_create_under_file_fails();
	test_name_dirsiz_boundary();
	TEST_PASS("easyfs_path");
	shutdown();
}
