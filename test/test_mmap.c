#include "user.h"
#include "libtest.h"

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20

static void test_mmap_anonymous_one_page(void)
{
	TEST_START("test_mmap_anonymous_one_page");

	char *page = mmap(0, 4096, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(page != (void *) -1, "test_mmap_anonymous_one_page",
		    "mmap should return a mapped page");

	page[0] = 'M';
	page[4095] = 'P';
	TEST_ASSERT(page[0] == 'M' && page[4095] == 'P',
		    "test_mmap_anonymous_one_page",
		    "mapped page should be readable and writable");

	TEST_PASS("test_mmap_anonymous_one_page");
}

void _start(void)
{
	TEST_START("mmap");
	test_mmap_anonymous_one_page();
	TEST_PASS("mmap");
	shutdown();
}
