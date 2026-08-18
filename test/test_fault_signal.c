#include "user.h"
#include "libtest.h"

#define TEST_NAME "fault_signal"

static void test_unmapped_access_kills_child(void)
{
	int pid = fork();
	TEST_ASSERT(pid >= 0, TEST_NAME, "fork faulting child");

	if (pid == 0) {
		volatile uint64 *bad = (volatile uint64 *) 0x40000000UL;
		*bad = 1;
		exit(1);
	}

	int status = 0;
	TEST_ASSERT(waitpid(pid, &status, 0) == pid, TEST_NAME,
		    "wait faulting child");
	TEST_ASSERT(status == ((128 + SIGSEGV) << 8), TEST_NAME,
		    "faulting child exits with SIGSEGV status");
	printf("FAULT_SIGSEGV_PARENT_SURVIVED\n");
}

void _start(void)
{
	TEST_START(TEST_NAME);
	test_unmapped_access_kills_child();
	TEST_PASS(TEST_NAME);
	shutdown();
}
