#include "user.h"
#include "libtest.h"

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20

static void test_large_mmap_allocates_on_touch(void)
{
	TEST_START("test_large_mmap_allocates_on_touch");

	char *pages = mmap(0, 256UL * 1024 * 1024, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(
	    pages != (void *) -1, "test_large_mmap_allocates_on_touch",
	    "lazy mmap should reserve a large range without eager allocation");

	pages[0] = 'L';
	TEST_ASSERT(pages[0] == 'L', "test_large_mmap_allocates_on_touch",
		    "first touched page should be allocated and writable");
	TEST_ASSERT(pages[4096] == 0, "test_large_mmap_allocates_on_touch",
		    "newly faulted lazy pages should be zero-filled");
	pages[128UL * 1024 * 1024] = 'Z';
	TEST_ASSERT(pages[128UL * 1024 * 1024] == 'Z',
		    "test_large_mmap_allocates_on_touch",
		    "a distant touched page should be allocated independently");
	TEST_ASSERT(
	    pages[0] == 'L', "test_large_mmap_allocates_on_touch",
	    "faulting a distant page should not disturb existing pages");
	TEST_ASSERT(munmap(pages, 256UL * 1024 * 1024) == 0,
		    "test_large_mmap_allocates_on_touch",
		    "munmap should release a sparsely populated lazy VMA");

	TEST_PASS("test_large_mmap_allocates_on_touch");
}

void _start(void)
{
	TEST_START("mmap_lazy");
	test_large_mmap_allocates_on_touch();
	TEST_PASS("mmap_lazy");
	shutdown();
}
