#include "user.h"

void _start(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		if (i > 1)
			printf(" ");
		printf("%s", argv[i]);
	}
	printf("\n");
	exit(0);
}
