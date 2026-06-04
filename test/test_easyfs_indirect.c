#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040
#define SEEK_SET 0
#define BSIZE 4096
#define NDIRECT 10

static char buf[BSIZE];

static void fill_pattern(char *p, int block, char mark)
{
	for (int i = 0; i < BSIZE; i++)
		p[i] = mark;
	p[0] = '0' + (block % 10);
	p[BSIZE - 1] = '0' + (block % 10);
}

static int check_pattern(char *p, int block, char mark)
{
	if (p[0] != '0' + (block % 10))
		return 0;
	if (p[BSIZE - 1] != '0' + (block % 10))
		return 0;
	for (int i = 1; i < BSIZE - 1; i++)
		if (p[i] != mark)
			return 0;
	return 1;
}

void _start(void)
{
	TEST_START("easyfs_indirect");

	int fd = open("/bigfile", O_WRONLY | O_CREAT);
	printf("open(/bigfile, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_indirect", "create bigfile");

	fill_pattern(buf, 0, 'D');
	long n = write(fd, buf, BSIZE);
	printf("write block 0 -> %d\n", (int) n);
	TEST_ASSERT(n == BSIZE, "easyfs_indirect", "write direct block 0");

	fill_pattern(buf, 5, 'D');
	lseek(fd, 5 * BSIZE, SEEK_SET);
	n = write(fd, buf, BSIZE);
	printf("write block 5 -> %d\n", (int) n);
	TEST_ASSERT(n == BSIZE, "easyfs_indirect", "write direct block 5");

	fill_pattern(buf, 9, 'D');
	lseek(fd, 9 * BSIZE, SEEK_SET);
	n = write(fd, buf, BSIZE);
	printf("write block 9 -> %d\n", (int) n);
	TEST_ASSERT(n == BSIZE, "easyfs_indirect", "write direct block 9");

	fill_pattern(buf, 10, 'I');
	lseek(fd, NDIRECT * BSIZE, SEEK_SET);
	n = write(fd, buf, BSIZE);
	printf("write block 10 (indirect) -> %d\n", (int) n);
	TEST_ASSERT(n == BSIZE, "easyfs_indirect", "write indirect block 10");

	close(fd);

	fd = open("/bigfile", O_RDONLY);
	printf("reopen /bigfile -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_indirect", "reopen bigfile");

	memset(buf, 0, BSIZE);
	lseek(fd, 0, SEEK_SET);
	n = read(fd, buf, BSIZE);
	printf("read block 0 -> %d\n", (int) n);
	TEST_ASSERT(n == BSIZE && check_pattern(buf, 0, 'D'), "easyfs_indirect",
		    "verify direct block 0");

	memset(buf, 0, BSIZE);
	lseek(fd, 5 * BSIZE, SEEK_SET);
	n = read(fd, buf, BSIZE);
	printf("read block 5 -> %d\n", (int) n);
	TEST_ASSERT(n == BSIZE && check_pattern(buf, 5, 'D'), "easyfs_indirect",
		    "verify direct block 5");

	memset(buf, 0, BSIZE);
	lseek(fd, 9 * BSIZE, SEEK_SET);
	n = read(fd, buf, BSIZE);
	printf("read block 9 -> %d\n", (int) n);
	TEST_ASSERT(n == BSIZE && check_pattern(buf, 9, 'D'), "easyfs_indirect",
		    "verify direct block 9");

	memset(buf, 0, BSIZE);
	lseek(fd, NDIRECT * BSIZE, SEEK_SET);
	n = read(fd, buf, BSIZE);
	printf("read block 10 (indirect) -> %d\n", (int) n);
	TEST_ASSERT(n == BSIZE && check_pattern(buf, 10, 'I'),
		    "easyfs_indirect", "verify indirect block 10");

	close(fd);

	TEST_PASS("easyfs_indirect");
	shutdown();
}
