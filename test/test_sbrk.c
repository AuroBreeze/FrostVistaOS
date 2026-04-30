// test_sbrk.c - Specialized test for heap allocation
#include "user.h"

void _start()
{
	printf("--- Testing sbrk growth ---\n");
	char *old_brk = sbrk(0);
	char *new_mem = sbrk(4096);

	if (new_mem == old_brk) {
		printf("Success: Allocated 4KB.\n");
		new_mem[0] = 'A'; // Verifying write access
	}

	printf("Test finished.\n");
	exit(0);
}
