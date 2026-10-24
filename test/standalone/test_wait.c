#include "user/user.h"
#include "user/libtest.h"

#define WNOHANG 1
#define WUNTRACED 2

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

static void test_waitpid_wuntraced(void)
{
	const char *name = "test_waitpid_wuntraced";
	TEST_START(name);
	int pid = fork();
	TEST_ASSERT(pid >= 0, name, "fork child");
	if (pid == 0)
		exit(11);
	int status = -1;
	TEST_ASSERT(waitpid(pid, &status, WUNTRACED) == pid, name,
		    "WUNTRACED still waits for normal child exit");
	TEST_ASSERT(status == (11 << 8), name, "preserve child exit status");
	TEST_PASS(name);
}

static void test_waitpid_wuntraced_wnohang(void)
{
	const char *name = "test_waitpid_wuntraced_wnohang";
	TEST_START(name);
	int fds[2];
	TEST_ASSERT(pipe2(fds, 0) == 0, name, "create child release pipe");
	int pid = fork();
	TEST_ASSERT(pid >= 0, name, "fork child");
	if (pid == 0) {
		close(fds[1]);
		char release;
		if (read(fds[0], &release, 1) != 1)
			exit(99);
		close(fds[0]);
		exit(5);
	}
	close(fds[0]);
	int status = -1;
	TEST_ASSERT(waitpid(pid, &status, WUNTRACED | WNOHANG) == 0, name,
		    "combined flags do not block while the child is alive");
	TEST_ASSERT(status == -1, name, "no event leaves status untouched");
	TEST_ASSERT(write(fds[1], "x", 1) == 1, name, "release child");
	close(fds[1]);
	TEST_ASSERT(waitpid(pid, &status, WUNTRACED) == pid, name,
		    "reap child after nonblocking wait");
	TEST_ASSERT(status == (5 << 8), name, "preserve child exit status");
	TEST_PASS(name);
}

int main(int argc, char **argv)
{
	TEST_START("wait");
	test_wait_any_child();
	test_waitpid_status();
	test_waitpid_wnohang();
	test_waitpid_wuntraced();
	test_waitpid_wuntraced_wnohang();
	TEST_PASS("wait");
	shutdown();
}
