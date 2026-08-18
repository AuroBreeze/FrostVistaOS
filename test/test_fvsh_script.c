#include "user.h"
#include "libtest.h"

#define MAX_ARGS 16
#define TEST_NAME "fvsh_script"

static int find_arg(char *argv[], char *ch)
{
	for (int i = 0; i < MAX_ARGS; i++) {
		if (argv[i] == 0)
			return -1;
		if (strcmp(argv[i], ch) == 0)
			return i;
	}
	return -1;
}

static void redirect_command(char *argv[])
{
	int idx_out = find_arg(argv, ">");
	int idx_in = find_arg(argv, "<");

	if (idx_out != -1) {
		int fd = open(argv[idx_out + 1], O_WRONLY | O_CREAT | O_TRUNC);
		TEST_ASSERT(fd >= 0, TEST_NAME, "open output redirection");
		TEST_ASSERT(dup3(fd, STDOUT_FILENO, 0) >= 0, TEST_NAME,
			    "dup stdout redirection");
		close(fd);
		argv[idx_out] = 0;
	}

	if (idx_in != -1) {
		int fd = open(argv[idx_in + 1], O_RDONLY);
		TEST_ASSERT(fd >= 0, TEST_NAME, "open input redirection");
		TEST_ASSERT(dup3(fd, STDIN_FILENO, 0) >= 0, TEST_NAME,
			    "dup stdin redirection");
		close(fd);
		argv[idx_in] = 0;
	}
}

static int syntax_error(char *argv[])
{
	int idx_pipe = find_arg(argv, "|");
	int idx_out = find_arg(argv, ">");
	int idx_in = find_arg(argv, "<");

	if (idx_pipe == 0 || (idx_pipe != -1 && argv[idx_pipe + 1] == 0)) {
		printf("bash: syntax error near unexpected token `|'\n");
		return 1;
	}
	if (idx_out != -1 && argv[idx_out + 1] == 0) {
		printf("bash: syntax error near unexpected token `>'\n");
		return 1;
	}
	if (idx_in != -1 && argv[idx_in + 1] == 0) {
		printf("bash: syntax error near unexpected token `<'\n");
		return 1;
	}

	return 0;
}

static int parse_args(char *line, char *argv[])
{
	int argc = 0;
	char *c = line;

	while (c && *c != '\0') {
		while (*c == ' ')
			c++;
		if (*c == '\0')
			break;
		if (argc >= MAX_ARGS - 1)
			break;
		argv[argc++] = c;
		while (*c != ' ' && *c != '\0')
			c++;
		if (*c == ' ')
			*c++ = '\0';
	}
	argv[argc] = 0;
	return argc;
}

static void copy_command(char *dst, const char *src)
{
	strncpy(dst, src, 256);
	dst[255] = '\0';
}

static void run_pipe(char *argv[], int idx_pipe)
{
	char *left[MAX_ARGS] = {0};
	char *right[MAX_ARGS] = {0};

	TEST_ASSERT(idx_pipe > 0, TEST_NAME, "pipe left side exists");
	TEST_ASSERT(argv[idx_pipe + 1] != 0, TEST_NAME,
		    "pipe right side exists");

	for (int i = 0; i < idx_pipe; i++)
		left[i] = argv[i];
	for (int i = idx_pipe + 1, j = 0; i < MAX_ARGS && argv[i] != 0;
	     i++, j++)
		right[j] = argv[i];

	int fds[2] = {-1, -1};
	TEST_ASSERT(pipe2(fds, 0) == 0, TEST_NAME, "pipe2");

	int pid1 = fork();
	TEST_ASSERT(pid1 >= 0, TEST_NAME, "fork pipe writer");
	if (pid1 == 0) {
		close(fds[0]);
		dup3(fds[1], STDOUT_FILENO, 0);
		close(fds[1]);
		redirect_command(left);
		execv(left[0], left);
		printf("fvsh: exec failed: %s\n", left[0]);
		exit(1);
	}

	int pid2 = fork();
	TEST_ASSERT(pid2 >= 0, TEST_NAME, "fork pipe reader");
	if (pid2 == 0) {
		close(fds[1]);
		dup3(fds[0], STDIN_FILENO, 0);
		close(fds[0]);
		redirect_command(right);
		execv(right[0], right);
		printf("fvsh: exec failed: %s\n", right[0]);
		exit(1);
	}

	close(fds[0]);
	close(fds[1]);
	wait();
	wait();
}

static void run_external(char *argv[])
{
	int idx_pipe = find_arg(argv, "|");
	if (idx_pipe != -1) {
		run_pipe(argv, idx_pipe);
		return;
	}

	int pid = fork();
	TEST_ASSERT(pid >= 0, TEST_NAME, "fork command");
	if (pid == 0) {
		redirect_command(argv);
		execv(argv[0], argv);
		printf("fvsh: exec failed: %s\n", argv[0]);
		exit(1);
	}
	wait();
}

static void run_line(const char *line)
{
	char buf[256] = {0};
	char *argv[MAX_ARGS] = {0};

	printf("fvsh-test> %s\n", line);
	copy_command(buf, line);
	int argc = parse_args(buf, argv);
	if (argc == 0)
		return;

	if (strcmp(argv[0], "pwd") == 0) {
		char cwd[128] = {0};
		TEST_ASSERT(getcwd(cwd, sizeof(cwd)) >= 0, TEST_NAME,
			    "pwd getcwd");
		printf("%s\n", cwd);
		return;
	}
	if (strcmp(argv[0], "cd") == 0) {
		TEST_ASSERT(argv[1] != 0, TEST_NAME, "cd operand");
		TEST_ASSERT(chdir(argv[1]) == 0, TEST_NAME, "cd");
		return;
	}
	if (syntax_error(argv))
		return;

	run_external(argv);
}

static void assert_file_content(const char *path, const char *expected)
{
	char buf[64] = {0};
	int fd = open(path, O_RDONLY);
	TEST_ASSERT(fd >= 0, TEST_NAME, "open expected file");

	long n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	TEST_ASSERT(n == (long) strlen(expected), TEST_NAME,
		    "expected file length");
	TEST_ASSERT(strncmp(buf, expected, strlen(expected)) == 0, TEST_NAME,
		    "expected file content");
}

/* Returns 1 if `name` is listed by readdir on the directory `path`. */
static int dir_contains(const char *path, const char *name)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return 0;

	char buf[2048];
	int found = 0;
	long n;
	while ((n = getdents64(fd, buf, sizeof(buf))) > 0) {
		char *p = buf;
		while (p < buf + n) {
			struct linux_dirent64 *d = (struct linux_dirent64 *) p;
			if (strcmp(d->d_name, name) == 0)
				found = 1;
			p += d->d_reclen;
		}
	}
	close(fd);
	return found;
}

/*
 * Round-trip a tmpfs file through the syscall ABI and verify the content
 * survives.  This is the bug trigger from the original report: the file is
 * written and closed (its icache slot is freed), then an easyfs binary is
 * exec'd, which reuses the icache slot and must not corrupt the tmpfs inode.
 */
static void tmpfs_write_read_roundtrip(const char *path, const char *content)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
	TEST_ASSERT(fd >= 0, TEST_NAME, "open tmpfs file for write");
	TEST_ASSERT(write(fd, content, strlen(content)) ==
			(long) strlen(content),
		    TEST_NAME, "write tmpfs file");
	close(fd);

	/* Force easyfs to reuse the just-freed icache slot. */
	int pid = fork();
	TEST_ASSERT(pid >= 0, TEST_NAME, "fork easyfs exec");
	if (pid == 0) {
		char *argv[] = {"/echo", "x", 0};
		execv("/echo", argv);
		exit(1);
	}
	wait();

	assert_file_content(path, content);
}

/* Write to a tmpfs file, then rewrite it shorter with O_TRUNC. */
static void tmpfs_truncate_rewrite(const char *path)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
	TEST_ASSERT(fd >= 0, TEST_NAME, "open for truncate rewrite");
	TEST_ASSERT(write(fd, "long content here\n", 18) == 18, TEST_NAME,
		    "write long content");
	close(fd);

	fd = open(path, O_WRONLY | O_TRUNC);
	TEST_ASSERT(fd >= 0, TEST_NAME, "reopen with O_TRUNC");
	TEST_ASSERT(write(fd, "x", 1) == 1, TEST_NAME, "rewrite short");
	close(fd);

	assert_file_content(path, "x");
}

/*
 * Create/delete/recreate a tmpfs file and verify the directory listing
 * reflects every step.  Exercises the unlink + readdir path that used to
 * leave a stale entry behind after the inode was corrupted.
 */
static void tmpfs_unlink_recreate(const char *path, const char *dir)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
	TEST_ASSERT(fd >= 0, TEST_NAME, "create for unlink test");
	close(fd);
	TEST_ASSERT(dir_contains(dir, "tu1"), TEST_NAME, "tu1 listed");

	TEST_ASSERT(unlink(path) == 0, TEST_NAME, "unlink tu1");
	TEST_ASSERT(!dir_contains(dir, "tu1"), TEST_NAME,
		    "tu1 gone after unlink");
	TEST_ASSERT(open(path, O_RDONLY) < 0, TEST_NAME,
		    "tu1 open fails after unlink");

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
	TEST_ASSERT(fd >= 0, TEST_NAME, "recreate tu1");
	TEST_ASSERT(write(fd, "again\n", 6) == 6, TEST_NAME, "write recreated");
	close(fd);
	assert_file_content(path, "again\n");
}

/*
 * Create several files in tmpfs, interleave easyfs execs between them, then
 * verify every file's content and the directory listing.  This is the core
 * regression test for the icache cross-filesystem private_data corruption.
 */
static void tmpfs_multi_file_stress(void)
{
	static const char *names[] = {"m0", "m1", "m2", "m3", "m4", 0};
	for (int i = 0; names[i] != 0; i++) {
		char path[64];
		strcpy(path, "/tmp/");
		strncpy(path + 5, names[i], sizeof(path) - 6);
		path[sizeof(path) - 1] = '\0';
		tmpfs_write_read_roundtrip(path, names[i]);
	}

	for (int i = 0; names[i] != 0; i++)
		TEST_ASSERT(dir_contains("/tmp", names[i]), TEST_NAME,
			    "stressed file listed");

	for (int i = 0; names[i] != 0; i++) {
		char path[64];
		strcpy(path, "/tmp/");
		strncpy(path + 5, names[i], sizeof(path) - 6);
		path[sizeof(path) - 1] = '\0';
		TEST_ASSERT(unlink(path) == 0, TEST_NAME, "unlink stressed");
		TEST_ASSERT(!dir_contains("/tmp", names[i]), TEST_NAME,
			    "stressed file gone");
	}
}

static void test_ctrl_c(void)
{
	int pid = fork();
	TEST_ASSERT(pid >= 0, TEST_NAME, "fork Ctrl+C child");
	if (pid == 0) {
		sleep(30);
		exit(0);
	}

	terminal_claim_input(pid);
	printf("FVSH_CTRL_C_READY\n");

	int status = 0;
	TEST_ASSERT(waitpid(pid, &status, 0) == pid, TEST_NAME,
		    "wait Ctrl+C child");
	TEST_ASSERT(status == (130 << 8), TEST_NAME,
		    "Ctrl+C child exits with SIGINT status");
	printf("FVSH_CTRL_C_SURVIVED\n");
}

void _start(void)
{
	static const char *commands[] = {
	    "pwd",
	    "echo smoke",
	    "echo >",
	    "echo 123 > test",
	    "cat test",
	    "cat fjasdlfk",
	    "cat < test",
	    "cat < test > copied_plain",
	    "cat < test | cat > copied",
	    "echo 1234 | cat",
	    "echo 1234 | cat > out",
	    "cat out",
	    "cd /tmp",
	    "/echo hello > f1",
	    "/cat f1",
	    "/ls",
	    "/echo one > t1",
	    "/echo two > t2",
	    "/echo three > t3",
	    "/cat t1",
	    "/cat t2",
	    "/cat t3",
	    "/ls",
	    "/rm t2",
	    "/ls",
	    "/echo two-again > t2",
	    "/cat t2",
	    "/ls",
	    0,
	};

	TEST_START(TEST_NAME);
	for (int i = 0; commands[i] != 0; i++)
		run_line(commands[i]);

	/* cd /tmp above changed the test process cwd; use absolute paths. */
	assert_file_content("/test", "123\n");
	assert_file_content("/copied_plain", "123\n");
	assert_file_content("/copied", "123\n");
	assert_file_content("/out", "1234\n");

	/* Direct tmpfs round-trip through the syscall ABI, independent of
	 * echo/cat so the file backend is tested on its own. */
	int tfd = open("/tmp/f2", O_WRONLY | O_CREAT);
	TEST_ASSERT(tfd >= 0, TEST_NAME, "open /tmp/f2");
	TEST_ASSERT(write(tfd, "hello\n", 6) == 6, TEST_NAME, "write /tmp/f2");
	close(tfd);
	tfd = open("/tmp/f2", O_RDONLY);
	TEST_ASSERT(tfd >= 0, TEST_NAME, "reopen /tmp/f2");
	char tbuf[16] = {0};
	long tn = read(tfd, tbuf, sizeof(tbuf));
	close(tfd);
	TEST_ASSERT(tn == 6 && strncmp(tbuf, "hello\n", 6) == 0, TEST_NAME,
		    "/tmp/f2 content hello\\n");
	TEST_ASSERT(unlink("/tmp/f2") == 0, TEST_NAME, "unlink /tmp/f2");
	TEST_ASSERT(open("/tmp/f2", O_RDONLY) < 0, TEST_NAME,
		    "/tmp/f2 gone after unlink");

	assert_file_content("/tmp/f1", "hello\n");

	TEST_ASSERT(unlink("/tmp/f1") == 0, TEST_NAME, "unlink /tmp/f1");
	int fd = open("/tmp/f1", O_RDONLY);
	TEST_ASSERT(fd < 0, TEST_NAME, "/tmp/f1 gone after rm");

	/* Shell-command tmpfs scenarios from the original bug report. */
	assert_file_content("/tmp/t1", "one\n");
	assert_file_content("/tmp/t3", "three\n");
	TEST_ASSERT(dir_contains("/tmp", "t2"), TEST_NAME, "t2 listed");
	/* /rm t2 above removed it, then /echo recreated it. */
	assert_file_content("/tmp/t2", "two-again\n");

	/* Direct-syscall tmpfs round-trips and truncate/unlink/recreate. */
	tmpfs_write_read_roundtrip("/tmp/rt1", "roundtrip\n");
	tmpfs_truncate_rewrite("/tmp/tr1");
	tmpfs_unlink_recreate("/tmp/tu1", "/tmp");
	tmpfs_multi_file_stress();
	test_ctrl_c();

	TEST_PASS(TEST_NAME);
	shutdown();
}
