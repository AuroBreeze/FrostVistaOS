#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR 0x002
#define O_CREAT 0x040
#define O_TRUNC 0x200
#define O_APPEND 0x400

#define CROSS_BLOCK_SIZE (4096 + 5)

static char cross_write_buf[CROSS_BLOCK_SIZE];
static char cross_read_buf[CROSS_BLOCK_SIZE + 1];

void _start(void)
{
	TEST_START("open");

	int fd = open("/dev/tty", O_RDONLY);
	printf("open(/dev/tty, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_RDONLY should open tty");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close O_RDONLY tty should succeed");

	fd = open("/dev/tty", O_WRONLY);
	printf("open(/dev/tty, O_WRONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_WRONLY should open tty");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close O_WRONLY tty should succeed");

	fd = open("/dev/tty", O_RDWR);
	printf("open(/dev/tty, O_RDWR) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_RDWR should open tty");
	TEST_ASSERT(close(fd) == 0, "open", "close O_RDWR tty should succeed");

	fd = open("/dev/tty", 3);
	printf("open(/dev/tty, invalid access mode 3) -> %d\n", fd);
	TEST_ASSERT(fd < 0, "open", "invalid access mode should fail");

	fd = open("/dev/tty", O_RDONLY | O_TRUNC);
	printf("open(/dev/tty, O_RDONLY|O_TRUNC) -> %d\n", fd);
	TEST_ASSERT(fd < 0, "open", "O_TRUNC with O_RDONLY should fail");

	fd = open("/oflags", O_WRONLY | O_CREAT);
	printf("open(/oflags, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_CREAT should create a missing file");
	char data[] = "hello";
	long n = write(fd, data, 5);
	printf("write(/oflags, hello) -> %d\n", (int) n);
	TEST_ASSERT(n == 5, "open", "write created file should succeed");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close written file should succeed");

	fd = open("/oflags", O_RDONLY);
	printf("open(/oflags, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "created file should reopen read-only");
	char buf[8] = {0};
	n = read(fd, buf, sizeof(buf));
	printf("read(/oflags) -> %d, '%s'\n", (int) n, buf);
	TEST_ASSERT(n == 5, "open", "reopened file should read written size");
	TEST_ASSERT(buf[0] == 'h' && buf[1] == 'e' && buf[2] == 'l' &&
			buf[3] == 'l' && buf[4] == 'o' && buf[5] == '\0',
		    "open", "reopened file should read written data");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close reopened file should succeed");

	fd = open("/oflags", O_WRONLY | O_TRUNC);
	printf("open(/oflags, O_WRONLY|O_TRUNC) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_TRUNC should open existing file");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close truncated file should succeed");

	fd = open("/oflags", O_RDONLY);
	printf("open(/oflags after truncate, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "truncated file should reopen read-only");
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	printf("read(/oflags after truncate) -> %d\n", (int) n);
	TEST_ASSERT(n == 0, "open", "truncated file should read EOF");
	TEST_ASSERT(buf[0] == '\0', "open",
		    "truncated file should not expose stale data");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close truncated reopen should succeed");

	fd = open("/append", O_WRONLY | O_CREAT);
	printf("open(/append, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_CREAT should create append test file");
	n = write(fd, "he", 2);
	printf("write(/append, he) -> %d\n", (int) n);
	TEST_ASSERT(n == 2, "open", "initial append test write should succeed");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close append test file should succeed");

	fd = open("/append", O_WRONLY | O_APPEND);
	printf("open(/append, O_WRONLY|O_APPEND) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_APPEND should open existing file");
	n = write(fd, "llo", 3);
	printf("write(/append, llo) -> %d\n", (int) n);
	TEST_ASSERT(n == 3, "open", "append write should succeed");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close append write file should succeed");

	fd = open("/append", O_RDONLY);
	printf("open(/append, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open",
		    "append test file should reopen read-only");
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	printf("read(/append) -> %d, '%s'\n", (int) n, buf);
	TEST_ASSERT(n == 5, "open", "append result should read full size");
	TEST_ASSERT(buf[0] == 'h' && buf[1] == 'e' && buf[2] == 'l' &&
			buf[3] == 'l' && buf[4] == 'o' && buf[5] == '\0',
		    "open", "append result should preserve existing data");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close append read file should succeed");

	fd = open("/fa", O_WRONLY | O_CREAT);
	printf("open(/fa, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_CREAT should create first file");
	n = write(fd, "aaa", 3);
	printf("write(/fa, aaa) -> %d\n", (int) n);
	TEST_ASSERT(n == 3, "open", "first file write should succeed");
	TEST_ASSERT(close(fd) == 0, "open", "close first file should succeed");

	fd = open("/fb", O_WRONLY | O_CREAT);
	printf("open(/fb, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_CREAT should create second file");
	n = write(fd, "bbb", 3);
	printf("write(/fb, bbb) -> %d\n", (int) n);
	TEST_ASSERT(n == 3, "open", "second file write should succeed");
	TEST_ASSERT(close(fd) == 0, "open", "close second file should succeed");

	fd = open("/fa", O_RDONLY);
	printf("open(/fa, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "first file should reopen read-only");
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	printf("read(/fa) -> %d, '%s'\n", (int) n, buf);
	TEST_ASSERT(n == 3, "open", "first file should read full size");
	TEST_ASSERT(buf[0] == 'a' && buf[1] == 'a' && buf[2] == 'a' &&
			buf[3] == '\0',
		    "open", "first file data should stay intact");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close first read file should succeed");

	fd = open("/fb", O_RDONLY);
	printf("open(/fb, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "second file should reopen read-only");
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf));
	printf("read(/fb) -> %d, '%s'\n", (int) n, buf);
	TEST_ASSERT(n == 3, "open", "second file should read full size");
	TEST_ASSERT(buf[0] == 'b' && buf[1] == 'b' && buf[2] == 'b' &&
			buf[3] == '\0',
		    "open", "second file data should stay intact");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close second read file should succeed");

	for (int i = 0; i < CROSS_BLOCK_SIZE; i++) {
		cross_write_buf[i] = 'x';
	}
	cross_write_buf[0] = 'A';
	cross_write_buf[4095] = 'B';
	cross_write_buf[4096] = 'C';
	cross_write_buf[CROSS_BLOCK_SIZE - 1] = 'D';

	fd = open("/cross", O_WRONLY | O_CREAT);
	printf("open(/cross, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_CREAT should create cross-block file");
	n = write(fd, cross_write_buf, CROSS_BLOCK_SIZE);
	printf("write(/cross, %d bytes) -> %d\n", CROSS_BLOCK_SIZE, (int) n);
	TEST_ASSERT(n == CROSS_BLOCK_SIZE, "open",
		    "cross-block write should complete");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close cross-block file should succeed");

	fd = open("/cross", O_RDONLY);
	printf("open(/cross, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open",
		    "cross-block file should reopen read-only");
	memset(cross_read_buf, 0, sizeof(cross_read_buf));
	n = read(fd, cross_read_buf, CROSS_BLOCK_SIZE);
	printf("read(/cross) -> %d\n", (int) n);
	TEST_ASSERT(n == CROSS_BLOCK_SIZE, "open",
		    "cross-block read should return full size");
	TEST_ASSERT(cross_read_buf[0] == 'A' && cross_read_buf[4095] == 'B' &&
			cross_read_buf[4096] == 'C' &&
			cross_read_buf[CROSS_BLOCK_SIZE - 1] == 'D',
		    "open", "cross-block boundary bytes should stay intact");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close cross-block read file should succeed");

	memset(cross_write_buf, 'y', sizeof(cross_write_buf));
	cross_write_buf[0] = 'E';
	cross_write_buf[4094] = 'F';

	fd = open("/appblk", O_WRONLY | O_CREAT);
	printf("open(/appblk, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_CREAT should create append-block file");
	n = write(fd, cross_write_buf, 4095);
	printf("write(/appblk, 4095 bytes) -> %d\n", (int) n);
	TEST_ASSERT(n == 4095, "open",
		    "initial append-block write should complete");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close append-block file should succeed");

	fd = open("/appblk", O_WRONLY | O_APPEND);
	printf("open(/appblk, O_WRONLY|O_APPEND) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open", "O_APPEND should open append-block file");
	n = write(fd, "GH", 2);
	printf("write(/appblk, GH) -> %d\n", (int) n);
	TEST_ASSERT(n == 2, "open",
		    "append-block write should cross block boundary");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close append-block write should succeed");

	fd = open("/appblk", O_RDONLY);
	printf("open(/appblk, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "open",
		    "append-block file should reopen read-only");
	memset(cross_read_buf, 0, sizeof(cross_read_buf));
	n = read(fd, cross_read_buf, 4097);
	printf("read(/appblk) -> %d\n", (int) n);
	TEST_ASSERT(n == 4097, "open",
		    "append-block read should return full size");
	TEST_ASSERT(cross_read_buf[0] == 'E' && cross_read_buf[4094] == 'F' &&
			cross_read_buf[4095] == 'G' &&
			cross_read_buf[4096] == 'H',
		    "open", "append-block boundary bytes should stay intact");
	TEST_ASSERT(close(fd) == 0, "open",
		    "close append-block read should succeed");

	TEST_PASS("open");
	shutdown();
}
