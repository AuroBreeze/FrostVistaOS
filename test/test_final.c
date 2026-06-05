#include "user.h"
#include "libtest.h"

#define TEST_FILE "/dev/tty"
#define HEAP_SIZE 4096
#define NUM_CHILDREN 3

static void test_memory_sbrk(void)
{
	printf("[1] Starting Memory Test (sbrk)...\n");

	char *p1 = sbrk(HEAP_SIZE);
	if (p1 == (void *) -1) {
		printf("FAILED: sbrk(4096) returned error.\n");
		TEST_FAIL("test_memory_sbrk");
	}

	for (int i = 0; i < HEAP_SIZE; i++) {
		p1[i] = i % 256;
	}

	for (int i = 0; i < HEAP_SIZE; i++) {
		if (p1[i] != (char) (i % 256)) {
			printf("FAILED: Data corruption at offset %d.\n", i);
			TEST_FAIL("test_memory_sbrk");
		}
	}
	printf("SUCCESS: Memory test passed.\n");
}

static void test_file_descriptors(void)
{
	printf("[2] Starting FD & IO Test...\n");

	int fd1 = open(TEST_FILE, 2);
	int fd2 = dup(fd1);
	int fd3 = dup(fd2);

	if (fd1 < 0 || fd2 < 0 || fd3 < 0) {
		printf("FAILED: open or dup failed.\n");
		TEST_FAIL("test_file_descriptors");
	}

	write(fd3, "Verify: writing via fd3 (duped from fd2 from fd1)\n", 50);

	close(fd2);
	if (write(fd1, "Verify: fd1 still working after close(fd2)\n", 42) <
	    0) {
		printf("FAILED: fd1 lost after close(fd2).\n");
		TEST_FAIL("test_file_descriptors");
	}

	close(fd1);
	close(fd3);
	printf("SUCCESS: FD and IO tests passed.\n");
}

static void child_task(int id)
{
	printf("Child %d: Starting task (allocating memory)...\n", id);
	char *buf = sbrk(1024);
	if (buf == (void *) -1)
		exit(1);

	for (int i = 0; i < 100; i++) {
		if (i % 20 == 0) {
			printf("Child %d: Progress %d%%\n", id, i);
		}
	}
	printf("Child %d: Task complete. Exiting.\n", id);
	exit(0);
}

static void test_multiprocess(void)
{
	printf("[3] Starting Multiprocess Test...\n");

	for (int i = 0; i < NUM_CHILDREN; i++) {
		int pid = fork();
		if (pid < 0) {
			printf("FAILED: fork failed at index %d.\n", i);
			TEST_FAIL("test_multiprocess");
		}
		if (pid == 0) {
			child_task(i + 1);
		}
	}

	for (int i = 0; i < NUM_CHILDREN; i++) {
		int exited_pid = wait();
		printf("Parent: Child process (PID %d) joined.\n", exited_pid);
	}
	printf("SUCCESS: Multiprocess test passed.\n");
}

void _start()
{
	TEST_START("final");
	printf("========== FrostVistaOS Comprehensive Test ==========\n");

	test_memory_sbrk();
	test_file_descriptors();
	test_multiprocess();

	printf("====================================================\n");
	printf("ALL TESTS PASSED: System core is stable.\n");
	printf("====================================================\n");
	TEST_PASS("final");

	shutdown();
}
