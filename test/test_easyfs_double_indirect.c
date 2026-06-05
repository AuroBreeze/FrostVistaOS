#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040
#define SEEK_SET 0
#define BSIZE 4096
#define NDIRECT 10
#define NINDIRECT (BSIZE / sizeof(uint32))
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

static void test_double_indirect_write(void)
{
	TEST_START("test_double_indirect_write");

	int fd = open("/hugefile", O_WRONLY | O_CREAT);
	printf("open(/hugefile) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_double_indirect_write", "create hugefile");

	TEST_ASSERT(seek_write(fd, 0, 'D', "direct"),
		    "test_double_indirect_write", "write direct blk 0");
	TEST_ASSERT(seek_write(fd, NDIRECT, 'S', "single"),
		    "test_double_indirect_write",
		    "write single-indirect boundary");
	TEST_ASSERT(seek_write(fd, DOUBLE_FIRST + 1, 'B', "double"),
		    "test_double_indirect_write", "write double-indirect blk");

	close(fd);
	TEST_PASS("test_double_indirect_write");
}

static void test_double_indirect_verify(void)
{
	TEST_START("test_double_indirect_verify");

	int fd = open("/hugefile", O_RDONLY);
	printf("reopen /hugefile -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_double_indirect_verify", "reopen hugefile");

	TEST_ASSERT(seek_read(fd, 0, 'D', "direct"),
		    "test_double_indirect_verify", "verify direct blk 0");
	TEST_ASSERT(seek_read(fd, NDIRECT, 'S', "single"),
		    "test_double_indirect_verify",
		    "verify single-indirect boundary");
	TEST_ASSERT(seek_read(fd, DOUBLE_FIRST + 1, 'B', "double"),
		    "test_double_indirect_verify",
		    "verify double-indirect blk");

	close(fd);
	TEST_PASS("test_double_indirect_verify");
}

void _start(void)
{
	TEST_START("easyfs_doubleindirect");
	test_double_indirect_write();
	test_double_indirect_verify();
	TEST_PASS("easyfs_doubleindirect");
	shutdown();
}
