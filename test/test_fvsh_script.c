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
	    0,
	};

	TEST_START(TEST_NAME);
	for (int i = 0; commands[i] != 0; i++)
		run_line(commands[i]);

	assert_file_content("test", "123\n");
	assert_file_content("copied_plain", "123\n");
	assert_file_content("copied", "123\n");
	assert_file_content("out", "1234\n");

	TEST_PASS(TEST_NAME);
	shutdown();
}
