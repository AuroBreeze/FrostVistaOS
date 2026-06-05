#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040

static void test_dirent_create_multiple(void)
{
	TEST_START("test_dirent_create_multiple");
	int fd = open("/a", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_dirent_create_multiple", "create /a");
	write(fd, "aaa", 3);
	close(fd);

	fd = open("/b", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_dirent_create_multiple", "create /b");
	write(fd, "bbb", 3);
	close(fd);

	fd = open("/c", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_dirent_create_multiple", "create /c");
	write(fd, "ccc", 3);
	close(fd);
	TEST_PASS("test_dirent_create_multiple");
}

static void test_dirent_unlink_reuses_slot(void)
{
	TEST_START("test_dirent_unlink_reuses_slot");
	int ret = unlink("/b");
	TEST_ASSERT(ret == 0, "test_dirent_unlink_reuses_slot", "unlink /b");

	int fd = open("/b", O_RDONLY);
	TEST_ASSERT(fd < 0, "test_dirent_unlink_reuses_slot",
		    "/b gone after unlink");

	fd = open("/new", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_dirent_unlink_reuses_slot", "create /new");
	write(fd, "zzz", 3);
	close(fd);

	fd = open("/a", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_dirent_unlink_reuses_slot",
		    "/a still exists");
	char buf[4] = {0};
	long n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 3 && buf[0] == 'a', "test_dirent_unlink_reuses_slot",
		    "/a data intact");
	close(fd);

	fd = open("/c", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_dirent_unlink_reuses_slot",
		    "/c still exists");
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 3 && buf[0] == 'c', "test_dirent_unlink_reuses_slot",
		    "/c data intact");
	close(fd);

	fd = open("/new", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_dirent_unlink_reuses_slot",
		    "/new should reuse /b's slot");
	n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 3 && buf[0] == 'z', "test_dirent_unlink_reuses_slot",
		    "/new data correct");
	close(fd);
	TEST_PASS("test_dirent_unlink_reuses_slot");
}

void _start(void)
{
	TEST_START("easyfs_dirent");
	test_dirent_create_multiple();
	test_dirent_unlink_reuses_slot();
	TEST_PASS("easyfs_dirent");
	shutdown();
}
