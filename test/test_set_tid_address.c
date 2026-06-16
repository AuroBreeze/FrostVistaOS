#include "user.h"
#include "libtest.h"

#define SYS_set_tid_address 96

static long raw_set_tid_address(int *tidptr)
{
	register long a0 __asm__("a0") = (long) tidptr;
	register long a7 __asm__("a7") = SYS_set_tid_address;
	__asm__ volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
	return a0;
}

static void test_returns_current_tid(void)
{
	TEST_START("test_returns_current_tid");
	int clear_child_tid = 0;
	long tid = raw_set_tid_address(&clear_child_tid);
	int pid = getpid();
	printf("set_tid_address -> %d, pid=%d\n", (int) tid, pid);
	TEST_ASSERT(tid == pid, "test_returns_current_tid",
		    "set_tid_address should return current tid");
	TEST_PASS("test_returns_current_tid");
}

void _start(void)
{
	TEST_START("set_tid_address");
	test_returns_current_tid();
	TEST_PASS("set_tid_address");
	shutdown();
}
