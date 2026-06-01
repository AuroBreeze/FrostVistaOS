// test_sbrk.c - Specialized test for heap allocation
#include "user.h"
#include "libtest.h"

void _start()
{
	TEST_START("sbrk");
	printf("--- Testing sbrk growth ---\n");
	char *old_brk = sbrk(0);
	char *new_mem = sbrk(4096);

	if (new_mem == old_brk) {
		new_mem[0] = 'A'; // Verifying write access
		printf("Success: Allocated 4KB.\n");
		TEST_PASS("sbrk");
	} else {
		TEST_FAIL("sbrk");
	}

	printf("Test finished.\n");
	shutdown();
}
