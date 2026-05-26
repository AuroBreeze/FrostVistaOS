#include "user.h"

// static const char *basic_tests[] = {
//     "brk",	  "chdir",	"clone",	"close",
//     "dup2",	  "dup",	"execve",	"exit",
//     "fork",	  "fstat",	"getcwd",	"getdents",
//     "getpid",	  "getppid",	"gettimeofday", "mkdir_",
//     "mmap",	  "mount",	"munmap",	"openat",
//     "open",	  "pipe",	"read",		"sleep",
//     "times",	  "umount",	"uname",	"unlink",
//     "wait",	  "waitpid",	"write",	"yield",
//     0,
// };

static const char *basic_tests[] = {
    "brk",   "getpid", "getppid", "write",	  "exit",   "fork",
    "wait",  "uname",  "times",	  "gettimeofday", "yield",  "sleep",

    "close", "dup",    "fstat",	  "open",	  "openat", "read",
    "dup2",  0,
};

static void run_one(const char *name)
{
	char path[64] = "/musl/basic/";
	int base_len = strlen(path);
	int i = 0;

	while (name[i] != '\0' && base_len + i < (int) sizeof(path) - 1) {
		path[base_len + i] = name[i];
		i++;
	}
	path[base_len + i] = '\0';

	printf("Testing %s :\n", name);

	int pid = fork();
	if (pid == 0) {
		exec(path);
		printf("exec failed: %s\n", path);
		exit(1);
	}

	wait();
}

void _start(void)
{
	printf("#### OS COMP TEST GROUP START basic-musl ####\n");

	for (int i = 0; basic_tests[i] != 0; i++) {
		run_one(basic_tests[i]);
	}

	printf("#### OS COMP TEST GROUP END basic-musl ####\n");
	shutdown();
}
