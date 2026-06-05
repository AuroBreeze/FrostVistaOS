#include "user.h"
#include "libtest.h"

#define O_RDWR 0x002

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

static void test_getcwd(void)
{
	TEST_START("test_getcwd");
	char cwd[128];
	char tiny[1];
	long ret = getcwd(0, sizeof(cwd));
	printf("getcwd(NULL, 128) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_getcwd", "getcwd null buffer should fail");
	ret = getcwd(cwd, 0);
	printf("getcwd(cwd, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_getcwd", "getcwd size 0 should fail");
	ret = getcwd(tiny, sizeof(tiny));
	printf("getcwd(tiny, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_getcwd", "getcwd tiny buffer should fail");
	ret = getcwd(cwd, sizeof(cwd));
	printf("getcwd(cwd, 128) -> %d, cwd=%s\n", (int) ret, cwd);
	TEST_ASSERT(ret == (long) cwd, "test_getcwd",
		    "getcwd should return user buffer");
	TEST_ASSERT(cwd[0] == '/', "test_getcwd", "cwd should be absolute");
	TEST_PASS("test_getcwd");
}

static void test_chdir(void)
{
	TEST_START("test_chdir");
	char cwd[128];
	long ret = chdir("");
	printf("chdir(empty) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_chdir", "chdir empty path should fail");
	ret = chdir("/");
	printf("chdir(/) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_chdir", "chdir root should succeed");
	ret = getcwd(cwd, sizeof(cwd));
	printf("getcwd(after chdir /) -> %d, cwd=%s\n", (int) ret, cwd);
	TEST_ASSERT(ret == (long) cwd, "test_chdir",
		    "getcwd after chdir should succeed");
	TEST_ASSERT(streq(cwd, "/"), "test_chdir", "cwd should be root");
	ret = chdir("/dev");
	printf("chdir(/dev) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_chdir", "chdir /dev should succeed");
	ret = chdir("/dev/tty");
	printf("chdir(/dev/tty) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_chdir", "chdir device should fail");
	ret = chdir("..");
	printf("chdir(.. from /dev) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_chdir",
		    "chdir .. from /dev should succeed");
	ret = getcwd(cwd, sizeof(cwd));
	printf("getcwd(after chdir ..) -> %d, cwd=%s\n", (int) ret, cwd);
	TEST_ASSERT(streq(cwd, "/"), "test_chdir", "cwd parent should be root");
	TEST_PASS("test_chdir");
}

static void test_gettimeofday(void)
{
	TEST_START("test_gettimeofday");
	struct linux_timeval tv;
	long ret = gettimeofday(0, 0);
	printf("gettimeofday(NULL, NULL) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_gettimeofday",
		    "gettimeofday null tv should succeed");
	ret = gettimeofday(&tv, 0);
	printf("gettimeofday(&tv, NULL) -> %d, sec=%d usec=%d\n", (int) ret,
	       (int) tv.tv_sec, (int) tv.tv_usec);
	TEST_ASSERT(ret == 0, "test_gettimeofday",
		    "gettimeofday valid tv should succeed");
	TEST_PASS("test_gettimeofday");
}

static void test_times(void)
{
	TEST_START("test_times");
	struct linux_tms tms;
	long ret = times(0);
	printf("times(NULL) -> %d\n", (int) ret);
	TEST_ASSERT(ret >= 0, "test_times", "times null tms should succeed");
	ret = times(&tms);
	printf("times(&tms) -> %d\n", (int) ret);
	TEST_ASSERT(ret >= 0, "test_times", "times valid tms should succeed");
	TEST_PASS("test_times");
}

static void test_uname(void)
{
	TEST_START("test_uname");
	struct linux_utsname uts;
	long ret = uname(&uts);
	printf("uname(&uts) -> %d, sysname=%s machine=%s\n", (int) ret,
	       uts.sysname, uts.machine);
	TEST_ASSERT(ret == 0, "test_uname", "uname should succeed");
	TEST_ASSERT(streq(uts.sysname, "FrostVistaOS"), "test_uname",
		    "uname sysname should match");
	ret = uname(0);
	printf("uname(NULL) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_uname", "uname null buffer should fail");
	TEST_PASS("test_uname");
}

static void test_nanosleep(void)
{
	TEST_START("test_nanosleep");
	struct linux_timespec ts = {0};
	struct linux_timespec bad_ts = {.tv_sec = 0, .tv_nsec = 1000000000};
	long ret = nanosleep(0);
	printf("nanosleep(NULL) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_nanosleep",
		    "nanosleep null req should fail");
	ret = nanosleep(&bad_ts);
	printf("nanosleep(bad nsec) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_nanosleep",
		    "nanosleep invalid nsec should fail");
	ret = nanosleep(&ts);
	printf("nanosleep(zero) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_nanosleep",
		    "nanosleep zero should succeed");
	TEST_PASS("test_nanosleep");
}

static void test_sched_stubs(void)
{
	TEST_START("test_sched_stubs");
	long ret = sched_yield();
	printf("sched_yield() -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_sched_stubs", "sched_yield should succeed");
	ret = setpriority(0, 0, 0);
	printf("setpriority(0,0,0) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_sched_stubs",
		    "setpriority stub should succeed");
	TEST_PASS("test_sched_stubs");
}

static void test_lseek_on_tty(void)
{
	TEST_START("test_lseek_on_tty");
	int fd = open("/dev/tty", O_RDWR);
	printf("open(/dev/tty, O_RDWR) -> %d\n", fd);
	TEST_ASSERT(fd >= 0, "test_lseek_on_tty", "open tty should succeed");
	long ret = lseek(-1, 0, 0);
	printf("lseek(-1, 0, SEEK_SET) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_lseek_on_tty",
		    "lseek invalid fd should fail");
	ret = lseek(fd, -1, 0);
	printf("lseek(fd, -1, SEEK_SET) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_lseek_on_tty",
		    "lseek negative target should fail");
	ret = lseek(fd, 0, 99);
	printf("lseek(fd, 0, 99) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_lseek_on_tty",
		    "lseek invalid whence should fail");
	ret = lseek(fd, 0, 1);
	printf("lseek(fd, 0, SEEK_CUR) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 0, "test_lseek_on_tty",
		    "lseek current offset should succeed");
	close(fd);
	TEST_PASS("test_lseek_on_tty");
}

static void test_dup3(void)
{
	TEST_START("test_dup3");
	int fd = open("/dev/tty", O_RDWR);
	TEST_ASSERT(fd >= 0, "test_dup3", "open tty should succeed");

	long ret = dup3(-1, 10, 0);
	printf("dup3(-1, 10, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_dup3", "dup3 invalid oldfd should fail");
	ret = dup3(fd, fd, 0);
	printf("dup3(fd, fd, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_dup3", "dup3 same fd should fail");
	ret = dup3(fd, 10, 1);
	printf("dup3(fd, 10, 1) -> %d\n", (int) ret);
	TEST_ASSERT(ret < 0, "test_dup3", "dup3 flags should fail");
	ret = dup3(fd, 10, 0);
	printf("dup3(fd, 10, 0) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 10, "test_dup3", "dup3 should duplicate fd");
	ret = write(10, "dup3 works\n", 11);
	printf("write(dup3 fd) -> %d\n", (int) ret);
	TEST_ASSERT(ret == 11, "test_dup3",
		    "dup3 target fd should be writable");
	close(10);
	close(fd);
	TEST_PASS("test_dup3");
}

void _start()
{
	TEST_START("sys_misc");
	test_getcwd();
	test_chdir();
	test_gettimeofday();
	test_times();
	test_uname();
	test_nanosleep();
	test_sched_stubs();
	test_lseek_on_tty();
	test_dup3();
	TEST_PASS("sys_misc");
	shutdown();
}
