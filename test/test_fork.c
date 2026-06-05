#include "user.h"
#include "libtest.h"

static void test_fork_basic(void)
{
	TEST_START("test_fork_basic");
	printf("--- Testing fork() ---\n");

	int parent_pid = getpid();
	int parent_ppid = getppid();
	printf("[Parent] getpid()=%d getppid()=%d\n", parent_pid, parent_ppid);
	TEST_ASSERT(parent_pid > 0, "test_fork_basic",
		    "parent getpid should be positive");
	TEST_ASSERT(parent_ppid >= 0, "test_fork_basic",
		    "parent getppid should be non-negative");

	int pid = fork();
	TEST_ASSERT(pid >= 0, "test_fork_basic", "fork should succeed");

	if (pid == 0) {
		int child_pid = getpid();
		int child_ppid = getppid();
		printf("[Child] getpid()=%d getppid()=%d\n", child_pid,
		       child_ppid);
		TEST_ASSERT(child_pid > 0, "test_fork_basic",
			    "child getpid should be positive");
		TEST_ASSERT(child_pid != parent_pid, "test_fork_basic",
			    "child pid should differ from parent pid");
		TEST_ASSERT(child_ppid == parent_pid, "test_fork_basic",
			    "child getppid should match parent pid");
		printf("[Child] Exiting...\n");
		exit(0);
	}

	int reaped_pid = wait();
	printf("[Parent] Child %d has been reaped.\n", reaped_pid);
	TEST_ASSERT(reaped_pid == pid, "test_fork_basic",
		    "wait should return the child pid");
	TEST_ASSERT(getpid() == parent_pid, "test_fork_basic",
		    "parent getpid should stay stable after wait");
	TEST_ASSERT(getppid() == parent_ppid, "test_fork_basic",
		    "parent getppid should stay stable after wait");
	TEST_PASS("test_fork_basic");
}

static void test_fork_memory_isolation(void)
{
	TEST_START("test_fork_memory_isolation");
	int shared_var = 100;
	printf("[Main] Initial shared_var = %d\n", shared_var);

	char *heap = sbrk(4096);
	TEST_ASSERT(heap != (void *) -1, "test_fork_memory_isolation",
		    "sbrk before fork should work");
	heap[0] = 'P';
	heap[4095] = 'Q';

	int pid = fork();
	TEST_ASSERT(pid >= 0, "test_fork_memory_isolation",
		    "fork should succeed");

	if (pid == 0) {
		printf("[Child] Hello from child process!\n");
		shared_var += 50;
		heap[0] = 'C';
		heap[4095] = 'D';
		printf("[Child] I modified shared_var to: %d\n", shared_var);
		printf("[Child] Exiting...\n");
		exit(0);
	}

	int reaped_pid = wait();
	printf("[Parent] Child %d has been reaped.\n", reaped_pid);
	TEST_ASSERT(reaped_pid == pid, "test_fork_memory_isolation",
		    "wait should return the child pid");

	if (shared_var == 100 && heap[0] == 'P' && heap[4095] == 'Q') {
		printf("--- fork() memory isolation test PASSED ---\n");
	} else {
		TEST_FAIL("test_fork_memory_isolation");
	}
	TEST_PASS("test_fork_memory_isolation");
}

static void test_fork_wait_no_children(void)
{
	TEST_START("test_fork_wait_no_children");
	int pid = fork();
	TEST_ASSERT(pid >= 0, "test_fork_wait_no_children",
		    "fork should succeed");

	if (pid == 0) {
		exit(0);
	}

	int reaped = wait();
	printf("[Parent] wait() -> %d\n", reaped);
	TEST_ASSERT(reaped == pid, "test_fork_wait_no_children",
		    "wait should return child pid");

	int wait_no_child = wait();
	printf("[Parent] wait() with no child -> %d\n", wait_no_child);
	TEST_ASSERT(wait_no_child < 0, "test_fork_wait_no_children",
		    "wait with no children should fail");
	TEST_PASS("test_fork_wait_no_children");
}

void _start()
{
	TEST_START("fork");
	test_fork_basic();
	test_fork_memory_isolation();
	test_fork_wait_no_children();
	TEST_PASS("fork");
	shutdown();
}
