#include "user.h"

static void cat_fd(int fd)
{
	char buf[256];

	while (1) {
		long n = read(fd, buf, sizeof(buf));
		if (n < 0) {
			printf("cat: read error\n");
			return;
		}
		if (n == 0)
			return;
		write(STDOUT_FILENO, buf, n);
	}
}

void _start(int argc, char **argv)
{
	if (argc == 1) {
		cat_fd(STDIN_FILENO);
		exit(0);
	}

	for (int i = 1; i < argc; i++) {
		int fd = open(argv[i], 0);
		if (fd < 0) {
			printf("cat: cannot open %s\n", argv[i]);
			continue;
		}

		cat_fd(fd);
		close(fd);
	}

	exit(0);
}
