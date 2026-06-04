#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040
#define SEEK_SET 0

#define BSIZE 4096
#define NDIRECT 10
#define NINDIRECT (BSIZE / sizeof(uint32))
#define SINGLE_FIRST NDIRECT
#define DOUBLE_FIRST (NDIRECT + NINDIRECT)

static char buf[BSIZE];

static void fill_block(char mark, int id)
{
	for (int i = 0; i < BSIZE; i++)
		buf[i] = mark;
	buf[0] = '0' + (id % 10);
	buf[BSIZE - 1] = '0' + (id % 10);
}

static int check_block(char mark, int id)
{
	if (buf[0] != '0' + (id % 10))
		return 0;
	if (buf[BSIZE - 1] != '0' + (id % 10))
		return 0;
	for (int i = 1; i < BSIZE - 1; i++)
		if (buf[i] != mark)
			return 0;
	return 1;
}

static int seek_write(int fd, int blk, char mark, const char *tag)
{
	fill_block(mark, blk);
	lseek(fd, (long) blk * BSIZE, SEEK_SET);
	long n = write(fd, buf, BSIZE);
	printf("write %s blk %d -> %d\n", tag, blk, (int) n);
	return n == BSIZE;
}

static int seek_read(int fd, int blk, char mark, const char *tag)
{
	memset(buf, 0, BSIZE);
	lseek(fd, (long) blk * BSIZE, SEEK_SET);
	long n = read(fd, buf, BSIZE);
	printf("read  %s blk %d -> %d\n", tag, blk, (int) n);
	return n == BSIZE && check_block(mark, blk);
}

void _start(void)
{
	TEST_START("easyfs_maxfile");

	int fd = open("/maxfile", O_WRONLY | O_CREAT);
	printf("open(/maxfile, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_maxfile", "create maxfile");

	TEST_ASSERT(seek_write(fd, 0, 'A', "direct-first"), "easyfs_maxfile",
		    "write first direct block");
	TEST_ASSERT(seek_write(fd, NDIRECT - 1, 'D', "direct-last"),
		    "easyfs_maxfile", "write last direct block");
	TEST_ASSERT(seek_write(fd, SINGLE_FIRST, 'S', "single-first"),
		    "easyfs_maxfile", "write first single-indirect block");
	TEST_ASSERT(seek_write(fd, DOUBLE_FIRST, 'B', "double-first"),
		    "easyfs_maxfile", "write first double-indirect block");
	TEST_ASSERT(close(fd) == 0, "easyfs_maxfile",
		    "close maxfile write fd should succeed");

	fd = open("/maxfile", O_RDONLY);
	printf("open(/maxfile, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_maxfile",
		    "maxfile should reopen read-only");

	TEST_ASSERT(seek_read(fd, 0, 'A', "direct-first"), "easyfs_maxfile",
		    "verify first direct block");
	TEST_ASSERT(seek_read(fd, NDIRECT - 1, 'D', "direct-last"),
		    "easyfs_maxfile", "verify last direct block");
	TEST_ASSERT(seek_read(fd, SINGLE_FIRST, 'S', "single-first"),
		    "easyfs_maxfile", "verify first single-indirect block");
	TEST_ASSERT(seek_read(fd, DOUBLE_FIRST, 'B', "double-first"),
		    "easyfs_maxfile", "verify first double-indirect block");
	TEST_ASSERT(close(fd) == 0, "easyfs_maxfile",
		    "close maxfile read fd should succeed");

	TEST_PASS("easyfs_maxfile");
	shutdown();
}
