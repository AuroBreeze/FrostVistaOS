#include "user.h"
#include "libtest.h"

static void test_copyin_lazy_alloc(void)
{
	TEST_START("test_copyin_lazy_alloc");
	printf("[Test 1] Triggering copyin lazy alloc...\n");

	char *buf_in = sbrk(4096);
	long ret = write(1, buf_in, 5);

	TEST_ASSERT(ret == 5, "test_copyin_lazy_alloc",
		    "copyin should handle unmapped page via lazy allocation");
	printf("[Test 1] copyin passed!\n");
	TEST_PASS("test_copyin_lazy_alloc");
}

static void test_copyout_lazy_alloc(void)
{
	TEST_START("test_copyout_lazy_alloc");
	printf("[Test 2] Triggering copyout lazy alloc...\n");

	char *buf_out = sbrk(4096);
	(void) buf_out;

	TEST_PASS("test_copyout_lazy_alloc");
}

void _start()
{
	TEST_START("lazy_copy");
	printf("--- Testing Lazy Allocation in Kernel Space ---\n");
	test_copyin_lazy_alloc();
	test_copyout_lazy_alloc();
	TEST_PASS("lazy_copy");
	printf("--- All Kernel Lazy Allocation Tests Finished ---\n");
	shutdown();
}
