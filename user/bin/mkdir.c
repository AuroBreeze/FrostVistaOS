#include "user.h"

void _start(int argc, char **argv)
{
	if (argc < 2) {
		printf("usage: mkdir <dir>...\n");
		exit(1);
	}

	for (int i = 1; i < argc; i++) {
		if (mkdir(argv[i], 0) < 0) {
			printf("mkdir: cannot create %s\n", argv[i]);
			continue;
		}
	}

	exit(0);
}
