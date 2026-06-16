#include "user.h"
#include "libtest.h"

struct linux_stat_user {
	uint64 st_dev;
	uint64 st_ino;
	uint32 st_mode;
	uint32 st_nlink;
	uint32 st_uid;
	uint32 st_gid;
	uint64 st_rdev;
	uint64 __pad1;
	int64 st_size;
	int st_blksize;
	int __pad2;
	int64 st_blocks;
	int64 st_atime_sec;
	int64 st_atime_nsec;
	int64 st_mtime_sec;
	int64 st_mtime_nsec;
	int64 st_ctime_sec;
	int64 st_ctime_nsec;
	uint32 __unused[2];
};

static long fstat_syscall(int fd, struct linux_stat_user *st)
{
	register long a0 __asm__("a0") = fd;
	register long a1 __asm__("a1") = (long) st;
	register long a7 __asm__("a7") = SYS_fstat;
	__asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
	return a0;
}

void _start(void)
{
	TEST_START("fstat");

	int fd = open("/fst", O_WRONLY | O_CREAT | O_TRUNC);
	TEST_ASSERT(fd >= 0, "fstat", "should create fstat test file");
	long n = write(fd, "abcdef", 6);
	TEST_ASSERT(n == 6, "fstat", "should write fstat test file");
	TEST_ASSERT(close(fd) == 0, "fstat", "should close written fstat file");

	fd = open("/fst", O_RDONLY);
	TEST_ASSERT(fd >= 0, "fstat", "should reopen fstat test file");

	struct linux_stat_user st = {0};
	TEST_ASSERT(fstat_syscall(fd, &st) == 0, "fstat",
		    "fstat syscall should succeed");
	TEST_ASSERT(st.st_size == 6, "fstat",
		    "Linux ABI st_size should report file size");
	TEST_ASSERT(close(fd) == 0, "fstat", "should close fstat read file");

	TEST_PASS("fstat");
	shutdown();
}
