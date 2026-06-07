#include "user.h"

void _start(int argc, char **argv)
{
	if (argc < 2) {
		printf("usage: rm <file>...\n");
		exit(1);
	}

	for (int i = 1; i < argc; i++) {
		if (unlink(argv[i]) < 0) {
			printf("rm: cannot remove %s\n", argv[i]);
			continue;
		}
	}

	exit(0);
}
