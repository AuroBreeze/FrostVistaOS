#include "user.h"
#include "libtest.h"

#define WNOHANG 1

static void test_wait_any_child(void)
{
	TEST_START("test_wait_any_child");

	int pid = fork();
	TEST_ASSERT(pid >= 0, "test_wait_any_child", "fork should succeed");

	if (pid == 0) {
		exit(0);
	}

	int reaped = wait();
	printf("wait() -> %d, child=%d\n", reaped, pid);
	TEST_ASSERT(reaped == pid, "test_wait_any_child",
		    "wait should return the child pid");
	TEST_PASS("test_wait_any_child");
}

static void test_waitpid_status(void)
{
	TEST_START("test_waitpid_status");

	int pid = fork();
	TEST_ASSERT(pid >= 0, "test_waitpid_status", "fork should succeed");

	if (pid == 0) {
		exit(7);
	}

	int status = 0;
	int reaped = waitpid(pid, &status, 0);
	printf("waitpid(%d) -> %d, status=%d\n", pid, reaped, status);
	TEST_ASSERT(reaped == pid, "test_waitpid_status",
		    "waitpid should return the requested child pid");
	TEST_ASSERT(status == (7 << 8), "test_waitpid_status",
		    "waitpid should copy the encoded exit status");
	TEST_PASS("test_waitpid_status");
}

static void test_waitpid_wnohang(void)
{
	TEST_START("test_waitpid_wnohang");

	int pid = fork();
	TEST_ASSERT(pid >= 0, "test_waitpid_wnohang", "fork should succeed");

	if (pid == 0) {
		for (int i = 0; i < 100; i++) {
			sched_yield();
		}
		exit(3);
	}

	int status = 0;
	int reaped = waitpid(pid, &status, WNOHANG);
	printf("waitpid(%d, WNOHANG) -> %d, status=%d\n", pid, reaped, status);

	if (reaped == 0) {
		reaped = waitpid(pid, &status, 0);
		printf("waitpid(%d) after WNOHANG -> %d, status=%d\n", pid,
		       reaped, status);
	}

	TEST_ASSERT(reaped == pid, "test_waitpid_wnohang",
		    "waitpid should eventually reap the child");
	TEST_ASSERT(status == (3 << 8), "test_waitpid_wnohang",
		    "waitpid should preserve status after WNOHANG");
	TEST_PASS("test_waitpid_wnohang");
}

void _start(void)
{
	TEST_START("wait");
	test_wait_any_child();
	test_waitpid_status();
	test_waitpid_wnohang();
	TEST_PASS("wait");
	shutdown();
}
