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

static int write_block(int fd, int blk, char mark, const char *tag)
{
	fill_block(mark, blk);
	lseek(fd, (long) blk * BSIZE, SEEK_SET);
	long n = write(fd, buf, BSIZE);
	printf("write %s blk %d -> %d\n", tag, blk, (int) n);
	return n == BSIZE;
}

static void verify_unlinked(const char *path)
{
	int fd = open(path, O_RDONLY);
	printf("open(%s) after unlink -> %d\n", path, fd);
	TEST_ASSERT(fd < 0, "verify_unlinked",
		    "file should be gone after unlink");
	if (fd >= 0)
		close(fd);
}

static void test_itrunc_single_indirect(void)
{
	TEST_START("test_itrunc_single_indirect");

	int fd = open("/sfile", O_WRONLY | O_CREAT);
	printf("create /sfile -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_itrunc_single_indirect",
		    "create single test file");
	TEST_ASSERT(write_block(fd, 0, 'D', "direct"),
		    "test_itrunc_single_indirect",
		    "write single test direct block");
	TEST_ASSERT(write_block(fd, SINGLE_FIRST, 'S', "single"),
		    "test_itrunc_single_indirect",
		    "write single test indirect block");
	close(fd);

	int ret = unlink("/sfile");
	printf("unlink /sfile -> %d\n", ret);
	TEST_ASSERT(ret == 0, "test_itrunc_single_indirect",
		    "unlink single test file");
	verify_unlinked("/sfile");
	TEST_PASS("test_itrunc_single_indirect");
}

static void test_itrunc_double_indirect(void)
{
	TEST_START("test_itrunc_double_indirect");

	int fd = open("/dfile", O_WRONLY | O_CREAT);
	printf("create /dfile -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_itrunc_double_indirect",
		    "create double test file");
	TEST_ASSERT(write_block(fd, 0, 'D', "direct"),
		    "test_itrunc_double_indirect",
		    "write double test direct block");
	TEST_ASSERT(write_block(fd, SINGLE_FIRST, 'S', "single"),
		    "test_itrunc_double_indirect",
		    "write double test single block");
	TEST_ASSERT(write_block(fd, DOUBLE_FIRST + 1, 'B', "double"),
		    "test_itrunc_double_indirect",
		    "write double test double block");
	close(fd);

	int ret = unlink("/dfile");
	printf("unlink /dfile -> %d\n", ret);
	TEST_ASSERT(ret == 0, "test_itrunc_double_indirect",
		    "unlink double test file");
	verify_unlinked("/dfile");
	TEST_PASS("test_itrunc_double_indirect");
}

static void test_itrunc_recreate(void)
{
	TEST_START("test_itrunc_recreate");

	int fd = open("/sfile", O_WRONLY | O_CREAT);
	printf("re-create /sfile -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_itrunc_recreate",
		    "re-create single after unlink");
	TEST_ASSERT(write_block(fd, 0, 'X', "direct"), "test_itrunc_recreate",
		    "write re-created direct block");
	close(fd);

	fd = open("/dfile2", O_WRONLY | O_CREAT);
	printf("create /dfile2 -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_itrunc_recreate",
		    "re-create double after unlink");
	TEST_ASSERT(write_block(fd, DOUBLE_FIRST + 1, 'R', "double-reuse"),
		    "test_itrunc_recreate",
		    "write reused double-indirect block");
	close(fd);
	TEST_PASS("test_itrunc_recreate");
}

void _start(void)
{
	TEST_START("easyfs_itrunc");
	test_itrunc_single_indirect();
	test_itrunc_double_indirect();
	test_itrunc_recreate();
	TEST_PASS("easyfs_itrunc");
	shutdown();
}
