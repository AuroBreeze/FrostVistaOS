#include "user.h"
#include "libtest.h"

static void test_brk_query(void)
{
	TEST_START("test_brk_query");
	char *base = (char *) brk(0);
	TEST_ASSERT(base == sbrk(0), "test_brk_query",
		    "brk(0) should match sbrk(0)");
	TEST_ASSERT((char *) brk(0) == base, "test_brk_query",
		    "brk(0) should be stable");
	TEST_PASS("test_brk_query");
}

static void test_brk_absolute_growth(void)
{
	TEST_START("test_brk_absolute_growth");
	char *base = (char *) brk(0);
	char *target = base + 4096;

	TEST_ASSERT((char *) brk(target) == target, "test_brk_absolute_growth",
		    "brk(base + 4096) should return the new break");
	base[0] = 'B';
	base[4095] = 'K';
	TEST_ASSERT(sbrk(0) == target, "test_brk_absolute_growth",
		    "break should move to the requested absolute address");
	TEST_PASS("test_brk_absolute_growth");
}

static void test_brk_shrink(void)
{
	TEST_START("test_brk_shrink");
	char *base = (char *) brk(0);
	char *target = base + 4096;

	TEST_ASSERT((char *) brk(target) == target, "test_brk_shrink",
		    "brk growth before shrink should succeed");
	TEST_ASSERT((char *) brk(base) == base, "test_brk_shrink",
		    "brk(base) should shrink back to base");
	TEST_ASSERT(sbrk(0) == base, "test_brk_shrink",
		    "break should equal base after shrink");
	TEST_PASS("test_brk_shrink");
}

static void test_brk_invalid_low(void)
{
	TEST_START("test_brk_invalid_low");
	char *base = (char *) brk(0);
	char *invalid = (char *) 1;

	TEST_ASSERT((char *) brk(invalid) == base, "test_brk_invalid_low",
		    "brk below heap bottom should return current break");
	TEST_ASSERT(sbrk(0) == base, "test_brk_invalid_low",
		    "invalid brk should not change current break");
	TEST_PASS("test_brk_invalid_low");
}

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
	char *base = sbrk(0);
	char *one_byte = sbrk(1);
	TEST_ASSERT(one_byte == base, "test_sbrk_small_growth",
		    "sbrk(1) should return old break");
	one_byte[0] = 'S';
	TEST_ASSERT(sbrk(0) == base + 1, "test_sbrk_small_growth",
		    "break should advance by 1 byte");
	TEST_PASS("test_sbrk_small_growth");
}

static void test_sbrk_page_growth(void)
{
	TEST_START("test_sbrk_page_growth");
	char *old_brk = sbrk(0);
	char *new_mem = sbrk(4096);

	TEST_ASSERT(new_mem == old_brk, "test_sbrk_page_growth",
		    "sbrk(4096) should return old break");
	new_mem[0] = 'A';
	new_mem[4095] = 'Z';
	TEST_ASSERT(sbrk(0) == old_brk + 4096, "test_sbrk_page_growth",
		    "break should advance by 4KB");
	TEST_PASS("test_sbrk_page_growth");
}

void _start()
{
	TEST_START("brk");
	test_brk_query();
	test_brk_absolute_growth();
	test_brk_shrink();
	test_brk_invalid_low();
	test_sbrk_query();
	test_sbrk_small_growth();
	test_sbrk_page_growth();
	TEST_PASS("brk");
	shutdown();
}
