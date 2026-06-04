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

void _start()
{
	printf("Hello from the shell!\n");

	while (1) {
		char buf[256] = {0};
		char *cmd = collect_char(buf);
		int len = strlen(buf);
		if (!is_blank(cmd)) {
			printf("Command: %s\n", cmd);
		}
		if (strcmp(cmd, "exit") == 0) {
			break;
		}
	}

	shutdown();
}
