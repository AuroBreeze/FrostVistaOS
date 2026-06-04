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

static void change_dir(char *cmd)
{
	char *path = cmd + 2;

	while (*path == ' ')
		path++;

	if (*path == '\0') {
		printf("cd: missing operand\n");
		return;
	}

	if (chdir(path) < 0)
		printf("cd: cannot change directory: %s\n", path);
}

void _start()
{
	printf("Hello from the shell!\n");

	while (1) {
		char buf[256] = {0};
		char *cmd = collect_char(buf);
		if (is_blank(cmd)) {
			continue;
		} else if (strcmp(cmd, "help") == 0) {
			print_help();
		} else if (strcmp(cmd, "pwd") == 0) {
			print_pwd();
		} else if (strncmp(cmd, "cd", 2) == 0 &&
			   (cmd[2] == '\0' || cmd[2] == ' ')) {
			change_dir(cmd);
		} else if (strcmp(cmd, "exit") == 0) {
			break;
		} else {
			printf("Unknown Command: %s\n", cmd);
		}
	}

	shutdown();
}
