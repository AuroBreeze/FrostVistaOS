#include "user.h"
#include "libtest.h"

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20

static void test_mmap_survives_fork(void)
{
	TEST_START("test_mmap_survives_fork");

	char *page = mmap(0, 4096, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(page != (void *) -1, "test_mmap_survives_fork",
		    "parent mmap should succeed before fork");

	page[0] = 'P';
	page[4095] = 'Q';

	int pid = fork();
	TEST_ASSERT(pid >= 0, "test_mmap_survives_fork", "fork should succeed");

	if (pid == 0) {
		TEST_ASSERT(page[0] == 'P' && page[4095] == 'Q',
			    "test_mmap_survives_fork",
			    "child should inherit readable mmap contents");
		page[0] = 'C';
		page[4095] = 'D';
		TEST_ASSERT(page[0] == 'C' && page[4095] == 'D',
			    "test_mmap_survives_fork",
			    "child should inherit writable mmap pages");
		TEST_ASSERT(munmap(page, 4096) == 0, "test_mmap_survives_fork",
			    "child should inherit mmap metadata for munmap");
		exit(0);
	}

	int status = 0;
	int reaped = waitpid(pid, &status, 0);
	TEST_ASSERT(reaped == pid, "test_mmap_survives_fork",
		    "waitpid should reap the mmap child");
	TEST_ASSERT(status == 0, "test_mmap_survives_fork",
		    "child should exit cleanly after mmap checks");
	TEST_ASSERT(page[0] == 'P' && page[4095] == 'Q',
		    "test_mmap_survives_fork",
		    "child writes should not modify parent private mmap");

	TEST_ASSERT(munmap(page, 4096) == 0, "test_mmap_survives_fork",
		    "parent munmap should release mmap after fork");

	TEST_PASS("test_mmap_survives_fork");
}

static void test_lazy_mmap_survives_fork(void)
{
	TEST_START("test_lazy_mmap_survives_fork");

	char *pages = mmap(0, 16UL * 1024 * 1024, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(
	    pages != (void *) -1, "test_lazy_mmap_survives_fork",
	    "parent lazy mmap should reserve a large range before fork");

	pages[0] = 'P';

	int pid = fork();
	TEST_ASSERT(pid >= 0, "test_lazy_mmap_survives_fork",
		    "fork should succeed with a lazy mmap VMA");

	if (pid == 0) {
		TEST_ASSERT(pages[0] == 'P', "test_lazy_mmap_survives_fork",
			    "child should inherit touched lazy mmap contents");
		pages[8UL * 1024 * 1024] = 'C';
		TEST_ASSERT(
		    pages[8UL * 1024 * 1024] == 'C',
		    "test_lazy_mmap_survives_fork",
		    "child should fault and allocate an untouched lazy page");
		TEST_ASSERT(munmap(pages, 16UL * 1024 * 1024) == 0,
			    "test_lazy_mmap_survives_fork",
			    "child should munmap inherited sparse VMA");
		exit(0);
	}

	int status = 0;
	int reaped = waitpid(pid, &status, 0);
	TEST_ASSERT(reaped == pid, "test_lazy_mmap_survives_fork",
		    "waitpid should reap the lazy mmap child");
	TEST_ASSERT(status == 0, "test_lazy_mmap_survives_fork",
		    "child should exit cleanly after lazy mmap checks");
	TEST_ASSERT(pages[0] == 'P', "test_lazy_mmap_survives_fork",
		    "child should not modify parent touched lazy page");
	TEST_ASSERT(pages[8UL * 1024 * 1024] == 0,
		    "test_lazy_mmap_survives_fork",
		    "parent untouched lazy page should remain zero-filled "
		    "after child fault");
	TEST_ASSERT(munmap(pages, 16UL * 1024 * 1024) == 0,
		    "test_lazy_mmap_survives_fork",
		    "parent should munmap sparse lazy VMA after fork");

	TEST_PASS("test_lazy_mmap_survives_fork");
}

void _start(void)
{
	TEST_START("mmap_fork");
	test_mmap_survives_fork();
	test_lazy_mmap_survives_fork();
	TEST_PASS("mmap_fork");
	shutdown();
}
