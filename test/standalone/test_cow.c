#include "user/user.h"
#include "user/libtest.h"

#define PAGE_SIZE 4096
#define NUM_PAGES 3
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20

static volatile char *alloc_pages(void)
{
	volatile char *pages =
	    mmap(0, PAGE_SIZE * NUM_PAGES, PROT_READ | PROT_WRITE,
		 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT((long) pages > 0, "cow", "private mmap should succeed");
	for (int i = 0; i < NUM_PAGES; i++) {
		pages[i * PAGE_SIZE] = 'A' + i;
		pages[(i + 1) * PAGE_SIZE - 1] = 'a' + i;
	}
	return pages;
}

static int original_pages(volatile char *pages)
{
	for (int i = 0; i < NUM_PAGES; i++) {
		if (pages[i * PAGE_SIZE] != 'A' + i ||
		    pages[(i + 1) * PAGE_SIZE - 1] != 'a' + i)
			return 0;
	}
	return 1;
}

static void reap_child(int pid)
{
	int status = -1;
	TEST_ASSERT(waitpid(pid, &status, 0) == pid && status == 0, "cow",
		    "child should complete its isolation checks");
}

static void test_cow_generations(void)
{
	TEST_START("test_cow_generations");
	volatile char *pages = alloc_pages();
	int gate[2];
	TEST_ASSERT(pipe2(gate, 0) == 0, "cow", "sync pipe should open");
	int pid = fork();
	TEST_ASSERT(pid >= 0, "cow", "first fork should succeed");
	if (pid == 0) {
		close(gate[1]);
		char token;
		/* Parent writes first; child must retain the pre-fork contents.
		 */
		if (read(gate[0], &token, 1) != 1 || !original_pages(pages))
			exit(1);
		close(gate[0]);
		/* Fork again while these data pages still have COW mappings. */
		int grandchild = fork();
		if (grandchild < 0)
			exit(2);
		if (grandchild == 0) {
			if (!original_pages(pages))
				exit(3);
			for (int i = 0; i < NUM_PAGES; i++) {
				pages[i * PAGE_SIZE] = 'G';
				pages[(i + 1) * PAGE_SIZE - 1] = 'g';
				if (pages[i * PAGE_SIZE] != 'G' ||
				    pages[(i + 1) * PAGE_SIZE - 1] != 'g')
					exit(4);
			}
			exit(munmap((void *) pages, PAGE_SIZE * NUM_PAGES) !=
			     0);
		}
		int status = -1;
		if (waitpid(grandchild, &status, 0) != grandchild ||
		    status != 0 || !original_pages(pages))
			exit(5);
		for (int i = 0; i < NUM_PAGES; i++)
			pages[i * PAGE_SIZE] = 'C';
		exit(0);
	}
	close(gate[0]);
	for (int i = 0; i < NUM_PAGES; i++)
		pages[i * PAGE_SIZE] = 'P';
	TEST_ASSERT(write(gate[1], "!", 1) == 1, "cow",
		    "parent should release the child after writing");
	close(gate[1]);
	reap_child(pid);
	for (int i = 0; i < NUM_PAGES; i++) {
		TEST_ASSERT(pages[i * PAGE_SIZE] == 'P' &&
				pages[(i + 1) * PAGE_SIZE - 1] == 'a' + i,
			    "cow",
			    "descendant writes must not alter parent pages");
	}
	TEST_ASSERT(munmap((void *) pages, PAGE_SIZE * NUM_PAGES) == 0, "cow",
		    "parent should release its mappings");
	TEST_PASS("test_cow_generations");
}

static void test_cow_copyout_and_reuse(void)
{
	TEST_START("test_cow_copyout_and_reuse");
	for (int round = 0; round < 16; round++) {
		volatile char *pages = alloc_pages();
		int fds[2];
		TEST_ASSERT(pipe2(fds, 0) == 0, "cow", "data pipe should open");
		TEST_ASSERT(write(fds[1], "XY", 2) == 2, "cow",
			    "pipe should hold data before fork");
		int pid = fork();
		TEST_ASSERT(pid >= 0, "cow", "repeated fork should succeed");
		if (pid == 0) {
			close(fds[1]);
			/* No user write precedes this cross-page kernel
			 * copyout. */
			if (read(fds[0], (void *) (pages + PAGE_SIZE - 1), 2) !=
				2 ||
			    pages[PAGE_SIZE - 1] != 'X' ||
			    pages[PAGE_SIZE] != 'Y')
				exit(1);
			close(fds[0]);
			exit(munmap((void *) pages, PAGE_SIZE * NUM_PAGES) !=
			     0);
		}
		close(fds[0]);
		close(fds[1]);
		reap_child(pid);
		TEST_ASSERT(
		    original_pages(pages), "cow",
		    "kernel copyout must preserve the parent's COW pages");
		/* Parent can still write after the other mappings are released.
		 */
		pages[PAGE_SIZE - 1] = 'Z';
		TEST_ASSERT(pages[PAGE_SIZE - 1] == 'Z', "cow",
			    "surviving mapping should remain writable after "
			    "child exit");
		TEST_ASSERT(munmap((void *) pages, PAGE_SIZE * NUM_PAGES) == 0,
			    "cow", "repeated mappings should unmap cleanly");
	}
	TEST_PASS("test_cow_copyout_and_reuse");
}

int main(int argc, char **argv)
{
	TEST_START("cow");
	test_cow_generations();
	test_cow_copyout_and_reuse();
	TEST_PASS("cow");
	shutdown();
}
