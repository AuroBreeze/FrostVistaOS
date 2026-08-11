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
    // "head text.txt",
    // "tail text.txt",
    // "hexdump -C text.txt",
    "clear",
    "sleep 1",
    "cat /musl/basic/text.txt",
    "md5sum /musl/basic/text.txt",
    // "df",
    // "dmesg",
    // "du",
    "grep hello busybox_cmd.txt",
    "printf \"abc\\n\"",
    // "ps",
    "cal",
    "dirname /aaa/bbb",
    "uptime",
    "echo \"#### independent command test\"",
    "echo \"#### file opration test\"",
    "basename /aaa/bbb",
    "ash -c exit",
    "sh -c exit",
    "pwd",
    "uname",
    "false",
    "true",
    "date",
    "expr 1 + 1",
    "ls",
    // "which ls",
    // "free",
    // "hwclock",
    // "sh -c 'sleep 5' & ./busybox kill $!",
    "touch /musl/basic/text.txt",
    "echo \"hello world\" > /musl/basic/text.txt",
    "cut -c 3 /musl/basic/text.txt",
    // "od /musl/basic/text.txt",        // needs lseek (unimplemented)
    "echo \"ccccccc\" >> /musl/basic/text.txt",
    "echo \"bbbbbbb\" >> /musl/basic/text.txt",
    "echo \"aaaaaaa\" >> /musl/basic/text.txt",
    "echo \"2222222\" >> /musl/basic/text.txt",
    "echo \"1111111\" >> /musl/basic/text.txt",
    "echo \"bbbbbbb\" >> /musl/basic/text.txt",
    "sort /musl/basic/text.txt | ./busybox uniq",
    "strings /musl/basic/text.txt",
    "wc /musl/basic/text.txt",
    "more /musl/basic/text.txt",
    "rm /musl/basic/text.txt",
    // "cp busybox_cmd.txt busybox_cmd.bak",   // needs stat (unimplemented)
    // "rm busybox_cmd.bak",
    0,
};

static const char *lua_scripts[] = {
    "date.lua",	     "file_io.lua", "max_min.lua", "random.lua",  "remove.lua",
    "round_num.lua", "sin30.lua",   "sort.lua",	   "strings.lua", 0,
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

/* Mirrors /musl/lua_testcode.sh + test.sh: run "./lua <script>" and print
 * "testcase lua <script> success|fail" from the exit status. */
static void run_lua_line(const char *name)
{
	char script[64];
	strcpy(script, "/musl/");
	strcpy(script + strlen(script), name);

	int pid = fork();
	if (pid == 0) {
		char *argv[] = {"/musl/lua", script, 0};
		char *envp[] = {0};
		execve("/musl/lua", argv, envp);
		printf("exec lua failed: %s\n", name);
		exit(127);
	}

	int status = 0;
	waitpid(pid, &status, 0);
	if (status == 0) {
		printf("testcase lua %s success\n", name);
	} else {
		printf("testcase lua %s fail\n", name);
	}
}

static void run_lua_group(const char *libc)
{
	char dir[64] = "/";
	strcpy(dir + strlen(dir), libc);

	printf("#### OS COMP TEST GROUP START lua-%s ####\n", libc);
	chdir(dir);

	for (int i = 0; lua_scripts[i] != 0; i++) {
		run_lua_line(lua_scripts[i]);
	}

	printf("#### OS COMP TEST GROUP END lua-%s ####\n", libc);
}

/* Mirrors /musl/libctest_testcode.sh: run the static and dynamic driver
 * scripts; runtest.exe prints each test case's result to stdout. */
static void run_libctest_script(char *script)
{
	int pid = fork();
	if (pid == 0) {
		char *argv[] = {"/musl/busybox", "sh", script, 0};
		char *envp[] = {0};
		execve("/musl/busybox", argv, envp);
		printf("exec libctest failed: %s\n", script);
		exit(127);
	}

	waitpid(pid, 0, 0);
}

static void run_libctest_group(const char *libc)
{
	char dir[64] = "/";
	strcpy(dir + strlen(dir), libc);

	printf("#### OS COMP TEST GROUP START libctest-%s ####\n", libc);
	chdir(dir);

	run_libctest_script("/musl/run-static.sh");
	run_libctest_script("/musl/run-dynamic.sh");

	printf("#### OS COMP TEST GROUP END libctest-%s ####\n", libc);
}

void _start(void)
{
	TEST_START("runner");
	// run_group("musl");
	// run_group("glibc");
	// run_busybox_group("musl");
	// run_lua_group("musl");
	run_libctest_group("musl");
	// run_busybox_group("glibc");
	TEST_PASS("runner");
	shutdown();
}
