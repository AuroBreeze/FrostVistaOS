#include "user.h"

void _start(int argc, char *argv[])
{
	if (argc == 1) {
		while (1) {
			int n = write(1, "y\n", 2);
			if (n < 0) {
				exit(1);
			}
		}
	}

	char str[256] = {0};
	for (int i = 1; i < argc; i++) {
		strncpy(str + strlen(str), argv[i], strlen(argv[i]));
		if (i != argc - 1) {
			strncpy(str + strlen(str), " ", 1);
		}
	}
	while (1) {
		int n = write(1, str, strlen(str));
		if (n < 0) {
			exit(1);
		}
		write(1, "\n", 1);
	}
	printf("\n");
	exit(0);
}
