#include "user.h"
#include "libtest.h"

static int run_busybox(char *const argv[], int expected_status,
		       const char *label)
{
	int pid = fork();
	TEST_ASSERT(pid >= 0, label, "fork should succeed");

	if (pid == 0) {
		char *envp[] = {0};
		execve("./busybox", argv, envp);
		printf("exec busybox failed: %s\n", label);
		exit(127);
	}

	int status = 0;
	int reaped = waitpid(pid, &status, 0);
	printf("testcase busybox %s status=%d\n", label, status);
	TEST_ASSERT(reaped == pid, label, "waitpid should reap busybox child");
	TEST_ASSERT(status == (expected_status << 8), label,
		    "busybox command should return expected status");
	return 0;
}

static void expect_busybox(char *const argv[], int expected_status,
			   const char *label)
{
	run_busybox(argv, expected_status, label);
	printf("testcase busybox %s success\n", label);
}

static void test_busybox_musl_smoke(void)
{
	TEST_START("test_busybox_musl_smoke");
	TEST_ASSERT(chdir("/musl") == 0, "test_busybox_musl_smoke",
		    "should enter /musl from sdcard image");

	printf("#### OS COMP TEST GROUP START busybox-musl ####\n");

	char *echo_argv[] = {"./busybox", "echo", "hello", 0};
	expect_busybox(echo_argv, 0, "echo hello");

	char *true_argv[] = {"./busybox", "true", 0};
	expect_busybox(true_argv, 0, "true");

	char *false_argv[] = {"./busybox", "false", 0};
	expect_busybox(false_argv, 1, "false");

	char *basename_argv[] = {"./busybox", "basename", "/aaa/bbb", 0};
	expect_busybox(basename_argv, 0, "basename /aaa/bbb");

	char *pwd_argv[] = {"./busybox", "pwd", 0};
	expect_busybox(pwd_argv, 0, "pwd");

	char *uname_argv[] = {"./busybox", "uname", 0};
	expect_busybox(uname_argv, 0, "uname");

	char *cat_argv[] = {"./busybox", "cat", "basic/text.txt", 0};
	expect_busybox(cat_argv, 0, "cat basic/text.txt");

	char *cut_argv[] = {"./busybox", "cut", "-c", "3", "basic/text.txt", 0};
	expect_busybox(cut_argv, 0, "cut -c 3 basic/text.txt");

	char *grep_argv[] = {"./busybox", "grep", "hello", "busybox_cmd.txt",
			     0};
	expect_busybox(grep_argv, 0, "grep hello busybox_cmd.txt");

	char *head_argv[] = {"./busybox", "head", "basic/text.txt", 0};
	expect_busybox(head_argv, 0, "head basic/text.txt");

	char *tail_argv[] = {"./busybox", "tail", "basic/text.txt", 0};
	expect_busybox(tail_argv, 0, "tail basic/text.txt");

	char *od_argv[] = {"./busybox", "od", "basic/text.txt", 0};
	expect_busybox(od_argv, 0, "od basic/text.txt");

	char *hexdump_argv[] = {"./busybox", "hexdump", "-C", "basic/text.txt",
				0};
	expect_busybox(hexdump_argv, 0, "hexdump -C basic/text.txt");

	char *md5sum_argv[] = {"./busybox", "md5sum", "basic/text.txt", 0};
	expect_busybox(md5sum_argv, 0, "md5sum basic/text.txt");

	char *stat_argv[] = {"./busybox", "stat", "basic/text.txt", 0};
	expect_busybox(stat_argv, 0, "stat basic/text.txt");

	char *strings_argv[] = {"./busybox", "strings", "basic/text.txt", 0};
	expect_busybox(strings_argv, 0, "strings basic/text.txt");

	char *wc_argv[] = {"./busybox", "wc", "basic/text.txt", 0};
	expect_busybox(wc_argv, 0, "wc basic/text.txt");

	printf("#### OS COMP TEST GROUP END busybox-musl ####\n");
	TEST_PASS("test_busybox_musl_smoke");
}

void _start(void)
{
	TEST_START("busybox");
	test_busybox_musl_smoke();
	TEST_PASS("busybox");
	shutdown();
}
