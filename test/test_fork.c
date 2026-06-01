#include "user.h"
#include "libtest.h"

void _start()
{
	TEST_START("fork");
	printf("--- Testing fork() ---\n");

	int shared_var = 100;
	int parent_pid = getpid();
	int parent_ppid = getppid();
	printf("[Parent] getpid()=%d getppid()=%d\n", parent_pid, parent_ppid);
	TEST_ASSERT(parent_pid > 0, "fork", "parent getpid should be positive");
	TEST_ASSERT(parent_ppid >= 0, "fork",
		    "parent getppid should be non-negative");
	char *heap = sbrk(4096);
	TEST_ASSERT(heap != (void *) -1, "fork", "sbrk before fork should work");
	heap[0] = 'P';
	heap[4095] = 'Q';
	printf("[Main] Initial shared_var = %d\n", shared_var);

	int pid = fork();

	if (pid < 0) {
		printf("[Error] fork failed!\n");
		exit(1);
	} else if (pid == 0) {
		printf("[Child] Hello from child process!\n");
		int child_pid = getpid();
		int child_ppid = getppid();
		printf("[Child] getpid()=%d getppid()=%d\n", child_pid,
		       child_ppid);
		TEST_ASSERT(child_pid > 0, "fork", "child getpid should be positive");
		TEST_ASSERT(child_pid != parent_pid, "fork",
			    "child pid should differ from parent pid");
		TEST_ASSERT(child_ppid == parent_pid, "fork",
			    "child getppid should match parent pid");

		shared_var += 50;
		heap[0] = 'C';
		heap[4095] = 'D';
		printf("[Child] I modified shared_var to: %d\n", shared_var);

		printf("[Child] Exiting...\n");
		exit(0);
	} else {
		printf("[Parent] Hello from parent process!\n");
		printf("[Parent] Created child with PID: %d\n", pid);

		printf("[Parent] Waiting for child to exit...\n");

		int reaped_pid = wait();

		printf("[Parent] Child %d has been reaped.\n", reaped_pid);

		printf("[Parent] My shared_var is still: %d (should be 100)\n",
		       shared_var);

		TEST_ASSERT(reaped_pid == pid, "fork",
			    "wait should return the child pid");
		TEST_ASSERT(getpid() == parent_pid, "fork",
			    "parent getpid should stay stable after wait");
		TEST_ASSERT(getppid() == parent_ppid, "fork",
			    "parent getppid should stay stable after wait");
		int wait_no_child = wait();
		printf("[Parent] wait() with no child -> %d\n", wait_no_child);
		TEST_ASSERT(wait_no_child < 0, "fork",
			    "wait with no children should fail");

		if (shared_var == 100 && heap[0] == 'P' && heap[4095] == 'Q') {
			printf("--- fork() memory isolation test PASSED ---\n");
			TEST_PASS("fork");
		} else {
			printf("--- fork() memory isolation test FAILED ---\n");
			TEST_FAIL("fork");
		}

		shutdown();
	}
}
