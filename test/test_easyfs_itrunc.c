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
	TEST_ASSERT(fd < 0, "easyfs_itrunc",
		    "file should be gone after unlink");
	if (fd >= 0)
		close(fd);
}

void _start(void)
{
	TEST_START("easyfs_itrunc");

	/* ── single-indirect unlink ─── */
	int fd = open("/sfile", O_WRONLY | O_CREAT);
	printf("create /sfile -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_itrunc", "create single test file");
	TEST_ASSERT(write_block(fd, 0, 'D', "direct"), "easyfs_itrunc",
		    "write single test direct block");
	TEST_ASSERT(write_block(fd, SINGLE_FIRST, 'S', "single"),
		    "easyfs_itrunc", "write single test indirect block");
	close(fd);

	fd = unlink("/sfile");
	printf("unlink /sfile -> %d\n", fd);
	TEST_ASSERT(fd == 0, "easyfs_itrunc", "unlink single test file");
	verify_unlinked("/sfile");

	/* ── double-indirect unlink ─── */
	fd = open("/dfile", O_WRONLY | O_CREAT);
	printf("create /dfile -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_itrunc", "create double test file");
	TEST_ASSERT(write_block(fd, 0, 'D', "direct"), "easyfs_itrunc",
		    "write double test direct block");
	TEST_ASSERT(write_block(fd, SINGLE_FIRST, 'S', "single"),
		    "easyfs_itrunc", "write double test single block");
	TEST_ASSERT(write_block(fd, DOUBLE_FIRST + 1, 'B', "double"),
		    "easyfs_itrunc", "write double test double block");
	close(fd);

	fd = unlink("/dfile");
	printf("unlink /dfile -> %d\n", fd);
	TEST_ASSERT(fd == 0, "easyfs_itrunc", "unlink double test file");
	verify_unlinked("/dfile");

	/* ── re-create after unlink (block reuse) ─── */
	fd = open("/sfile", O_WRONLY | O_CREAT);
	printf("re-create /sfile -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_itrunc", "re-create single after unlink");
	TEST_ASSERT(write_block(fd, 0, 'X', "direct"), "easyfs_itrunc",
		    "write re-created direct block");
	close(fd);

	fd = open("/dfile2", O_WRONLY | O_CREAT);
	printf("create /dfile2 -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_itrunc", "re-create double after unlink");
	TEST_ASSERT(write_block(fd, DOUBLE_FIRST + 1, 'R', "double-reuse"),
		    "easyfs_itrunc", "write reused double-indirect block");
	close(fd);

	TEST_PASS("easyfs_itrunc");
	shutdown();
}
