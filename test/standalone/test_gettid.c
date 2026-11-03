#include "user/user.h"
#include "asm/user.h"
#include "user/libtest.h"

int main(int argc, char **argv)
{
	TEST_START("gettid");
	int parent_tid = gettid();
	TEST_ASSERT(parent_tid > 0 && parent_tid == getpid(), "gettid",
		    "single-threaded process TID should equal its PID");
	TEST_ASSERT(arch_user_syscall3(178, 0, 0, 0) == parent_tid, "gettid",
		    "Linux syscall 178 should return the current TID");

	int pid = fork();
	TEST_ASSERT(pid >= 0, "gettid", "fork should succeed");
	if (pid == 0) {
		int tid = gettid();
		int valid = tid > 0 && tid == getpid() && tid != parent_tid;
		sched_yield();
		exit(valid && gettid() == tid ? 0 : 1);
	}

	int status = -1;
	TEST_ASSERT(waitpid(pid, &status, 0) == pid && status == 0, "gettid",
		    "child should retain its own TID across scheduling");
	TEST_ASSERT(gettid() == parent_tid, "gettid",
		    "child creation should not change the parent's TID");
	TEST_PASS("gettid");
	shutdown();
}
