// test_sbrk.c - Specialized test for heap allocation
#include "user.h"
#include "libtest.h"

void _start()
{
	TEST_START("sbrk");
	char *base = sbrk(0);
	TEST_ASSERT(sbrk(0) == base, "sbrk", "sbrk(0) should be stable");

	printf("--- Testing sbrk small growth ---\n");
	char *one_byte = sbrk(1);
	TEST_ASSERT(one_byte == base, "sbrk", "sbrk(1) should return old break");
	one_byte[0] = 'B'; // Verifying sub-page write access
	TEST_ASSERT(sbrk(0) == base + 1, "sbrk", "break should advance by 1 byte");

	printf("--- Testing sbrk growth ---\n");
	char *old_brk = sbrk(0);
	char *new_mem = sbrk(4096);

	if (new_mem == old_brk) {
		new_mem[0] = 'A'; // Verifying write access
		new_mem[4095] = 'Z'; // Verifying access at the end of the page
		TEST_ASSERT(sbrk(0) == old_brk + 4096, "sbrk",
			    "break should advance by 4KB");
		printf("Success: Allocated 4KB.\n");
		TEST_PASS("sbrk");
	} else {
		TEST_FAIL("sbrk");
	}

	printf("Test finished.\n");
	shutdown();
}
