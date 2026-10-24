#include "user/user.h"
#include "user/libtest.h"

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

int main(int argc, char **argv)
{
	if (argc == 3 && streq(argv[0], "execv-child")) {
		TEST_START("execv-child");
		printf("child argc=%d argv0=%s argv1=%s argv2=%s\n", argc,
		       argv[0], argv[1], argv[2]);
		TEST_ASSERT(streq(argv[1], "hello"), "execv-child",
			    "argv[1] should survive execv");
		TEST_ASSERT(streq(argv[2], "world"), "execv-child",
			    "argv[2] should survive execv");
		TEST_PASS("execv-child");
		return 37;
	}

	TEST_START("execv");
	int pid = fork();
	if (pid == 0) {
		char *child_argv[] = {"execv-child", "hello", "world", 0};
		execv("/init", child_argv);
		printf("execv failed\n");
		return 1;
	}

	TEST_ASSERT(pid > 0, "execv", "fork should succeed");
	int status = 0;
	TEST_ASSERT(waitpid(pid, &status, 0) == pid, "execv",
		    "waitpid should collect the exec child");
	TEST_ASSERT(status == (37 << 8), "execv",
		    "CRT should pass the main return value to exit");
	TEST_PASS("execv");
	shutdown();
}
