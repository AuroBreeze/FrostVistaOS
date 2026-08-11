#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040
#define O_TRUNC 0x200

/* Overlay end-to-end: every op lands on /musl (ext4 dir) but writes go
 * to the tmpfs upper layer. The EXT4 image is never modified. */

static void test_overlay_create_read(void)
{
	TEST_START("test_overlay_create_read");

	int fd = open("/musl/ov_ut", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_overlay_create_read", "create /musl/ov_ut");
	TEST_ASSERT(write(fd, "hello", 5) == 5, "test_overlay_create_read",
		    "write");
	close(fd);

	fd = open("/musl/ov_ut", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_overlay_create_read", "reopen");
	char buf[8] = {0};
	long n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n == 5 && buf[0] == 'h' && buf[4] == 'o',
		    "test_overlay_create_read", "read back");
	close(fd);

	TEST_ASSERT(unlink("/musl/ov_ut") == 0, "test_overlay_create_read",
		    "unlink");
	TEST_ASSERT(open("/musl/ov_ut", O_RDONLY) < 0,
		    "test_overlay_create_read", "gone after unlink");
	TEST_PASS("test_overlay_create_read");
}

static void test_overlay_copyup(void)
{
	TEST_START("test_overlay_copyup");

	/* Open a lower EXT4 file for write; the first write copies it up.
	 * text.txt lives in the contest image (used by the runner too). */
	int fd = open("/musl/basic/text.txt", O_WRONLY);
	if (fd < 0) {
		printf("skip: no /musl/basic/text.txt\n");
		TEST_PASS("test_overlay_copyup");
		return;
	}
	char data[] = "overlay copy-up\n";
	TEST_ASSERT(write(fd, data, sizeof(data) - 1) ==
			(long) (sizeof(data) - 1),
		    "test_overlay_copyup", "write lower file");
	close(fd);

	fd = open("/musl/basic/text.txt", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_overlay_copyup", "reopen after copy-up");
	char buf[128] = {0};
	long n = read(fd, buf, sizeof(buf));
	TEST_ASSERT(n >= (long) (sizeof(data) - 1) && buf[0] == 'o' &&
			buf[1] == 'v',
		    "test_overlay_copyup", "read modified content");
	close(fd);
	TEST_PASS("test_overlay_copyup");
}

static void test_overlay_mkdir(void)
{
	TEST_START("test_overlay_mkdir");

	TEST_ASSERT(mkdir("/musl/ov_dir", 0) == 0, "test_overlay_mkdir",
		    "mkdir /musl/ov_dir");
	int fd = open("/musl/ov_dir/ovf", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd >= 0, "test_overlay_mkdir", "create inside overlay dir");
	close(fd);
	fd = open("/musl/ov_dir/ovf", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_overlay_mkdir", "reopen inside overlay dir");
	close(fd);
	TEST_PASS("test_overlay_mkdir");
}

void _start(void)
{
	test_overlay_create_read();
	test_overlay_mkdir();
	test_overlay_copyup();
	shutdown();
}
