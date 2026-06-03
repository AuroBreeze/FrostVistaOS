#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040

#define BSIZE 4096
#define NDIRECT 12
#define MAXFILE_SIZE (BSIZE * NDIRECT)

static char write_buf[MAXFILE_SIZE];
static char read_buf[MAXFILE_SIZE];

void _start(void)
{
	TEST_START("easyfs_maxfile");

	for (int i = 0; i < MAXFILE_SIZE; i++) {
		write_buf[i] = 'm';
	}
	write_buf[0] = 'A';
	write_buf[BSIZE - 1] = 'B';
	write_buf[BSIZE] = 'C';
	write_buf[MAXFILE_SIZE - 1] = 'D';

	int fd = open("/maxfile", O_WRONLY | O_CREAT);
	printf("open(/maxfile, O_WRONLY|O_CREAT) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_maxfile", "O_CREAT should create maxfile");

	long n = write(fd, write_buf, MAXFILE_SIZE);
	printf("write(/maxfile, %d bytes) -> %d\n", MAXFILE_SIZE, (int) n);
	TEST_ASSERT(n == MAXFILE_SIZE, "easyfs_maxfile",
		    "max direct-block write should complete");
	TEST_ASSERT(close(fd) == 0, "easyfs_maxfile",
		    "close maxfile write fd should succeed");

	fd = open("/maxfile", O_RDONLY);
	printf("open(/maxfile, O_RDONLY) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "easyfs_maxfile",
		    "maxfile should reopen read-only");

	memset(read_buf, 0, sizeof(read_buf));
	n = read(fd, read_buf, MAXFILE_SIZE);
	printf("read(/maxfile) -> %d\n", (int) n);
	TEST_ASSERT(n == MAXFILE_SIZE, "easyfs_maxfile",
		    "max direct-block read should return full size");
	TEST_ASSERT(
	    read_buf[0] == 'A' && read_buf[BSIZE - 1] == 'B' &&
		read_buf[BSIZE] == 'C' && read_buf[MAXFILE_SIZE - 1] == 'D',
	    "easyfs_maxfile", "maxfile boundary bytes should stay intact");
	TEST_ASSERT(close(fd) == 0, "easyfs_maxfile",
		    "close maxfile read fd should succeed");

	TEST_PASS("easyfs_maxfile");
	shutdown();
}
