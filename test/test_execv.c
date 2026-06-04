#include "user.h"
#include "libtest.h"

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

void _start(int argc, char **argv)
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
		exit(0);
	}

	TEST_START("execv");
	int pid = fork();
	if (pid == 0) {
		char *child_argv[] = {"execv-child", "hello", "world", 0};
		execv("/init", child_argv);
		printf("execv failed\n");
		exit(1);
	}

	TEST_ASSERT(pid > 0, "execv", "fork should succeed");
	wait();
	TEST_PASS("execv");
	shutdown();
}
