#include "user.h"

void _start(int argc, char **argv)
{
	if (argc < 2) {
		printf("usage: touch <file>...\n");
		exit(1);
	}

	for (int i = 1; i < argc; i++) {
		int fd = open(argv[i], O_WRONLY | O_CREAT);
		if (fd < 0) {
			printf("touch: cannot touch %s\n", argv[i]);
			continue;
		}
		close(fd);
	}

	exit(0);
}
