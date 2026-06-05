#include "user.h"
#include "libtest.h"

static void test_sbrk_query(void)
{
	TEST_START("test_sbrk_query");
	char *base = sbrk(0);
	TEST_ASSERT(sbrk(0) == base, "test_sbrk_query",
		    "sbrk(0) should be stable");
	TEST_PASS("test_sbrk_query");
}

static void test_sbrk_small_growth(void)
{
	TEST_START("test_sbrk_small_growth");
	printf("--- Testing sbrk small growth ---\n");
	char *base = sbrk(0);
	char *one_byte = sbrk(1);
	TEST_ASSERT(one_byte == base, "test_sbrk_small_growth",
		    "sbrk(1) should return old break");
	one_byte[0] = 'B';
	TEST_ASSERT(sbrk(0) == base + 1, "test_sbrk_small_growth",
		    "break should advance by 1 byte");
	TEST_PASS("test_sbrk_small_growth");
}

static void test_sbrk_page_growth(void)
{
	TEST_START("test_sbrk_page_growth");
	printf("--- Testing sbrk growth ---\n");
	char *old_brk = sbrk(0);
	char *new_mem = sbrk(4096);

	TEST_ASSERT(new_mem == old_brk, "test_sbrk_page_growth",
		    "sbrk(4096) should return old break");
	new_mem[0] = 'A';
	new_mem[4095] = 'Z';
	TEST_ASSERT(sbrk(0) == old_brk + 4096, "test_sbrk_page_growth",
		    "break should advance by 4KB");
	printf("Success: Allocated 4KB.\n");
	TEST_PASS("test_sbrk_page_growth");
}

void _start()
{
	TEST_START("sbrk");
	test_sbrk_query();
	test_sbrk_small_growth();
	test_sbrk_page_growth();
	TEST_PASS("sbrk");
	printf("Test finished.\n");
	shutdown();
}
