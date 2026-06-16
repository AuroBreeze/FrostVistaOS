#include "user.h"
#include "libtest.h"

#define SYS_setgid 144
#define SYS_setuid 146
#define SYS_getuid 174
#define SYS_getgid 176

static long raw_syscall1(long num, long arg0)
{
	register long a0 __asm__("a0") = arg0;
	register long a7 __asm__("a7") = num;
	__asm__ volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
	return a0;
}

static long raw_getuid(void)
{
	return raw_syscall1(SYS_getuid, 0);
}

static long raw_getgid(void)
{
	return raw_syscall1(SYS_getgid, 0);
}

static long raw_setgid(long gid)
{
	return raw_syscall1(SYS_setgid, gid);
}

static long raw_setuid(long uid)
{
	return raw_syscall1(SYS_setuid, uid);
}

static void test_getuid_returns_root(void)
{
	TEST_START("test_getuid_returns_root");
	long uid = raw_getuid();
	printf("getuid -> %d\n", (int) uid);
	TEST_ASSERT(uid == 0, "test_getuid_returns_root",
		    "getuid should return root uid in single-user mode");
	TEST_PASS("test_getuid_returns_root");
}

static void test_getgid_returns_root(void)
{
	TEST_START("test_getgid_returns_root");
	long gid = raw_getgid();
	printf("getgid -> %d\n", (int) gid);
	TEST_ASSERT(gid == 0, "test_getgid_returns_root",
		    "getgid should return root gid in single-user mode");
	TEST_PASS("test_getgid_returns_root");
}

static void test_setid_accepts_root_identity(void)
{
	TEST_START("test_setid_accepts_root_identity");
	long setgid_ret = raw_setgid(0);
	long setuid_ret = raw_setuid(0);
	printf("setgid(0) -> %d, setuid(0) -> %d\n", (int) setgid_ret,
	       (int) setuid_ret);
	TEST_ASSERT(setgid_ret == 0, "test_setid_accepts_root_identity",
		    "setgid(0) should succeed in single-user mode");
	TEST_ASSERT(setuid_ret == 0, "test_setid_accepts_root_identity",
		    "setuid(0) should succeed in single-user mode");
	TEST_PASS("test_setid_accepts_root_identity");
}

void _start(void)
{
	TEST_START("getuid");
	test_getuid_returns_root();
	test_getgid_returns_root();
	test_setid_accepts_root_identity();
	TEST_PASS("getuid");
	shutdown();
}
