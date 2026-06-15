#include "user.h"
#include "libtest.h"

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define NVMA_TEST_SLOTS 16

static int streq(const char *a, const char *b)
{
	int i = 0;
	while (a[i] != '\0' && b[i] != '\0') {
		if (a[i] != b[i])
			return 0;
		i++;
	}
	return a[i] == b[i];
}

static void test_child_after_exec(void)
{
	TEST_START("mmap_execve_child");

	char *page = mmap(0, 4096, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(page != (void *) -1, "mmap_execve_child",
		    "execve should clear stale mmap VMA slots");

	page[0] = 'E';
	TEST_ASSERT(page[0] == 'E', "mmap_execve_child",
		    "new mmap after execve should be usable");
	TEST_ASSERT(munmap(page, 4096) == 0, "mmap_execve_child",
		    "munmap should release the post-exec mapping");

	TEST_PASS("mmap_execve_child");
	shutdown();
}

static void test_exec_failure_preserves_mmap(void)
{
	TEST_START("mmap_execve_failure_preserves_mmap");

	char *page = mmap(0, 4096, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(page != (void *) -1, "mmap_execve_failure_preserves_mmap",
		    "mmap should succeed before failed execve");

	page[0] = 'F';
	char *bad_argv[] = {"missing", 0};
	TEST_ASSERT(execv("/missing-exec-target", bad_argv) == -1,
		    "mmap_execve_failure_preserves_mmap",
		    "execv should fail for a missing path");
	TEST_ASSERT(page[0] == 'F', "mmap_execve_failure_preserves_mmap",
		    "failed execve should preserve existing mmap contents");
	page[0] = 'G';
	TEST_ASSERT(page[0] == 'G', "mmap_execve_failure_preserves_mmap",
		    "failed execve should preserve mmap writability");
	TEST_ASSERT(munmap(page, 4096) == 0,
		    "mmap_execve_failure_preserves_mmap",
		    "failed execve should preserve mmap metadata for munmap");

	TEST_PASS("mmap_execve_failure_preserves_mmap");
}

void _start(int argc, char **argv)
{
	if (argc == 2 && streq(argv[1], "child"))
		test_child_after_exec();

	TEST_START("mmap_execve");
	test_exec_failure_preserves_mmap();

	for (int i = 0; i < NVMA_TEST_SLOTS; i++) {
		char *page = mmap(0, 4096, PROT_READ | PROT_WRITE,
				  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		TEST_ASSERT(
		    page != (void *) -1, "mmap_execve",
		    "parent should fill all mmap VMA slots before execve");
		page[0] = 'P';
	}

	char *child_argv[] = {"mmap_execve", "child", 0};
	execv("/init", child_argv);
	TEST_ASSERT(0, "mmap_execve", "execv should not return on success");
}
