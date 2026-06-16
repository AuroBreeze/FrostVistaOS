#include "user.h"
#include "libtest.h"

static const char *basic_tests[] = {
    "brk",	"chdir",  "close",   "dup2",	     "dup",
    "execve",	"exit",	  "fork",    "fstat",	     "getcwd",
    "getdents", "getpid", "getppid", "gettimeofday", "mkdir_",
    "mmap",	"mount",  "munmap",  "openat",	     "open",
    "pipe",	"read",	  "sleep",   "times",	     "umount",
    "uname",	"unlink", "wait",    "waitpid",	     "write",
    "yield",	0,
};

static void run_one(const char *name)
{
	char path[64] = "./";
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

		// NOTE: By default, it is unreachable after being completely
		// overridden by exec.
		printf("exec failed: %s\n", path);
		exit(1);
	}

	wait();
}

static void run_group(const char *libc)
{
	char dir[64] = "/";
	strcpy(dir + strlen(dir), libc);
	strcpy(dir + strlen(dir), "/basic");

	printf("#### OS COMP TEST GROUP START basic-%s ####\n", libc);
	chdir(dir);

	for (int i = 0; basic_tests[i] != 0; i++) {
		run_one(basic_tests[i]);
	}

	printf("#### OS COMP TEST GROUP END basic-%s ####\n", libc);
}

static const char *busybox_cmds[] = {
    // "df",
    // "dirname /aaa/bbb",
    // "dmesg",
    // "du",
    // "grep hello busybox_cmd.txt",
    "echo \"#### independent command test\"",
    "echo \"#### file opration test\"",
    "basename /aaa/bbb",
    "ash -c exit",
    "sh -c exit",
    "clear",
    "pwd",
    "uname",
    "false",
    "true",
    // "sleep 1",
    "date",
    "expr 1 + 1",
    // "cal",
    // "which ls",
    // "uptime",
    // "printf \"abc\\n\"",
    // "ps",
    // "free",
    // "hwclock",
    // "sh -c 'sleep 5' & ./busybox kill $!",
    // "ls",
    // "touch test.txt",
    // "echo \"hello world\" > test.txt",
    // "cat test.txt",
    // "cut -c 3 test.txt",
    // "od test.txt",
    // "head test.txt",
    // "tail test.txt",
    // "hexdump -C test.txt",
    // "md5sum test.txt",
    // "echo \"ccccccc\" >> test.txt",
    // "echo \"bbbbbbb\" >> test.txt",
    // "echo \"aaaaaaa\" >> test.txt",
    // "echo \"2222222\" >> test.txt",
    // "echo \"1111111\" >> test.txt",
    // "echo \"bbbbbbb\" >> test.txt",
    // "sort test.txt | ./busybox uniq",
    // "stat test.txt",
    // "strings test.txt",
    // "wc test.txt",
    // "[ -f test.txt ]",
    // "more test.txt",
    // "rm test.txt",
    // "mkdir test_dir",
    // "mv test_dir test",
    // "rmdir test",
    // "cp busybox_cmd.txt busybox_cmd.bak",
    // "rm busybox_cmd.bak",
    // "find -name \"busybox_cmd.txt\"",
    0,
};

static void make_busybox_shell_cmd(char *dst, const char *line)
{
	strcpy(dst, "/musl/busybox ");
	strcpy(dst + strlen(dst), line);
}

static void run_busybox_line(const char *line)
{
	char cmd[128];
	make_busybox_shell_cmd(cmd, line);

	int pid = fork();
	if (pid == 0) {
		char *argv[] = {"/musl/busybox", "sh", "-c", cmd, 0};
		char *envp[] = {0};
		execve("/musl/busybox", argv, envp);
		printf("exec busybox failed: %s\n", line);
		exit(127);
	}

	int status = 0;
	waitpid(pid, &status, 0);
	if (status == 0 || strcmp(line, "false") == 0) {
		printf("testcase busybox %s success\n", line);
	} else {
		printf("testcase busybox %s fail\n", line);
	}
}

static void run_busybox_group(const char *libc)
{
	char dir[64] = "/";
	strcpy(dir + strlen(dir), libc);

	printf("#### OS COMP TEST GROUP START busybox-%s ####\n", libc);
	chdir(dir);

	for (int i = 0; busybox_cmds[i] != 0; i++) {
		run_busybox_line(busybox_cmds[i]);
	}

	printf("#### OS COMP TEST GROUP END busybox-%s ####\n", libc);
}

void _start(void)
{
	TEST_START("runner");
	run_group("musl");
	run_group("glibc");
	run_busybox_group("musl");
	TEST_PASS("runner");
	shutdown();
}
