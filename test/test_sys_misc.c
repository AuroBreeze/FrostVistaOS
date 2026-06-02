#include "user.h"
#include "libtest.h"

#define O_RDWR 0x002
#define SEEK_SET 0
#define SEEK_CUR 1

static int streq(const char *a, const char *b)
{
	int i = 0;
	while (a[i] != '\0' && b[i] != '\0') {
		if (a[i] != b[i])
			return 0;
		i++;
	}
	return a[i] == b[i];
}

void _start()
{
	TEST_START("sys_misc");
	long ret;

	char cwd[128];
	char tiny[1];
	ret = getcwd(0, sizeof(cwd));
	printf("getcwd(NULL, 128) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "getcwd null buffer should fail");
	ret = getcwd(cwd, 0);
	printf("getcwd(cwd, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "getcwd size 0 should fail");
	ret = getcwd(tiny, sizeof(tiny));
	printf("getcwd(tiny, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "getcwd tiny buffer should fail");
	ret = getcwd(cwd, sizeof(cwd));
	printf("getcwd(cwd, 128) -> %d, cwd=%s\n", (int) ret, cwd);
	TEST_ASSERT(ret == (long) cwd, "sys_misc",
		    "getcwd should return user buffer");
	TEST_ASSERT(cwd[0] == '/', "sys_misc", "cwd should be absolute");
	ret = chdir("");
	printf("chdir(empty) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "chdir empty path should fail");
	ret = chdir("/");
	printf("chdir(/) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_misc", "chdir root should succeed");
	ret = getcwd(cwd, sizeof(cwd));
	printf("getcwd(after chdir /) -> %d, cwd=%s\n", (int) ret, cwd);
	TEST_ASSERT(ret == (long) cwd, "sys_misc",
		    "getcwd after chdir should succeed");
	TEST_ASSERT(streq(cwd, "/"), "sys_misc", "cwd should be root");

	struct linux_timeval tv;
	struct linux_tms tms;
	struct linux_utsname uts;
	struct linux_timespec ts = {0};
	struct linux_timespec bad_ts = {.tv_sec = 0, .tv_nsec = 1000000000};

	ret = gettimeofday(0, 0);
	printf("gettimeofday(NULL, NULL) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_misc",
		    "gettimeofday null tv should succeed");
	ret = gettimeofday(&tv, 0);
	printf("gettimeofday(&tv, NULL) -> %d, sec=%d usec=%d\n", (int) ret,
	       (int) tv.tv_sec, (int) tv.tv_usec);
	TEST_ASSERT(ret == 0, "sys_misc",
		    "gettimeofday valid tv should succeed");
	ret = times(0);
	printf("times(NULL) -> %d\n", (int) ret);
	TEST_ASSERT(ret >= 0, "sys_misc", "times null tms should succeed");
	ret = times(&tms);
	printf("times(&tms) -> %d\n", (int) ret);
	TEST_ASSERT(ret >= 0, "sys_misc", "times valid tms should succeed");
	ret = uname(&uts);
	printf("uname(&uts) -> %d, sysname=%s machine=%s\n", (int) ret,
	       uts.sysname, uts.machine);
	TEST_ASSERT(ret == 0, "sys_misc", "uname should succeed");
	TEST_ASSERT(streq(uts.sysname, "FrostVistaOS"), "sys_misc",
		    "uname sysname should match");
	ret = uname(0);
	printf("uname(NULL) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "uname null buffer should fail");

	ret = nanosleep(0);
	printf("nanosleep(NULL) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "nanosleep null req should fail");
	ret = nanosleep(&bad_ts);
	printf("nanosleep(bad nsec) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "nanosleep invalid nsec should fail");
	ret = nanosleep(&ts);
	printf("nanosleep(zero) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_misc", "nanosleep zero should succeed");
	ret = sched_yield();
	printf("sched_yield() -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_misc", "sched_yield should succeed");
	ret = setpriority(0, 0, 0);
	printf("setpriority(0,0,0) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_misc", "setpriority stub should succeed");

	int fd = open("/dev/tty", O_RDWR);
	printf("open(/dev/tty, O_RDWR) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "sys_misc", "open tty should succeed");
	ret = lseek(-1, 0, SEEK_SET);
	printf("lseek(-1, 0, SEEK_SET) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "lseek invalid fd should fail");
	ret = lseek(fd, -1, SEEK_SET);
	printf("lseek(fd, -1, SEEK_SET) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "lseek negative target should fail");
	ret = lseek(fd, 0, 99);
	printf("lseek(fd, 0, 99) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "lseek invalid whence should fail");
	ret = lseek(fd, 0, SEEK_CUR);
	printf("lseek(fd, 0, SEEK_CUR) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_misc",
		    "lseek current offset should succeed");

	ret = dup3(-1, 10, 0);
	printf("dup3(-1, 10, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "dup3 invalid oldfd should fail");
	ret = dup3(fd, fd, 0);
	printf("dup3(fd, fd, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "dup3 same fd should fail");
	ret = dup3(fd, 10, 1);
	printf("dup3(fd, 10, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc", "dup3 flags should fail");
	ret = dup3(fd, 10, 0);
	printf("dup3(fd, 10, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 10, "sys_misc", "dup3 should duplicate fd");
	ret = write(10, "dup3 works\n", 11);
	printf("write(dup3 fd) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 11, "sys_misc", "dup3 target fd should be writable");
	ret = close(10);
	printf("close(10) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_misc", "close dup3 target should succeed");
	ret = close(fd);
	printf("close(source fd) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "sys_misc", "close source fd should succeed");

	int pipefds[2];
	ret = pipe2(pipefds, 0);
	printf("pipe2(pipefds, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "sys_misc",
		    "pipe2 is not implemented yet and should fail");

	TEST_PASS("sys_misc");
	shutdown();
}
