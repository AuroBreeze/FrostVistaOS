#include "user.h"

char *collect_char(char *buf)
{
	int len = 0;

	while (1) {
		printf("fvsh> ");
		while (1) {
			char ch[2] = {0};
			int n = read(0, &ch, 1);
			ch[1] = '\0';

			if (n > 0) {
				if (ch[0] == '\r' || ch[0] == '\n') {
					printf("\r\n");
					return buf;
				} else if (ch[0] == '\b' || ch[0] == 0x7f) {
					if (len > 0) {
						buf[--len] = '\0';
						printf("\b \b");
					}
					continue;
				} else {
					buf[len++] = ch[0];
					buf[len] = '\0';
				}

				printf("%s", ch);
				if (len >= 255) {
					printf("\nBuffer overflow!\n");
					return buf;
				}
			} else if (n < 0) {
				printf("Read error!\n");
				break;
			}
		}
	}
}

int is_blank(char *buf)
{
	int len = strlen(buf);
	if (len == 0)
		return 1;

	for (int i = 0; i < len; i++) {
		if (buf[i] != ' ')
			return 0;
	}

	return 1;
}

static void print_help(void)
{
	printf("FrostVista shell commands:\n");
	printf("  cd    - change current directory\n");
	printf("  help  - show this help message\n");
	printf("  pwd   - print current directory\n");
	printf("  exit  - leave the shell\n");
}

static void print_pwd(void)
{
	char cwd[256] = {0};
	long ret = getcwd(cwd, sizeof(cwd));

	if (ret < 0) {
		printf("pwd: getcwd failed\n");
		return;
	}

	printf("%s\n", cwd);
}

static void change_dir(char *path)
{
	if (path == 0 || *path == '\0') {
		printf("cd: missing operand\n");
		return;
	}

	if (chdir(path) < 0)
		printf("cd: cannot change directory: %s\n", path);
}

static void run_external(char *argv[])
{
	int pid = fork();

	if (pid < 0) {
		printf("fvsh: fork failed\n");
		return;
	}

	if (pid == 0) {
		execv(argv[0], argv);
		printf("fvsh: exec failed: %s\n", argv[0]);
		exit(1);
	}

	wait();
}

#define MAX_ARGS 16

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

void _start()
{
	printf("Hello from the FrostVista shell!\n");

	while (1) {
		char buf[256] = {0};
		char *argv[MAX_ARGS] = {0};
		char *cmd = collect_char(buf);
		int argc = parse_args(cmd, argv);

		if (argc == 0) {
			continue;
		} else if (strcmp(argv[0], "help") == 0) {
			print_help();
		} else if (strcmp(argv[0], "pwd") == 0) {
			print_pwd();
		} else if (strcmp(argv[0], "cd") == 0) {
			change_dir(argv[1]);
		} else if (strcmp(argv[0], "exit") == 0) {
			break;
		} else {
			run_external(argv);
		}
	}

	shutdown();
}
