#include "user.h"

static void list_dir(const char *path)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf("ls: cannot open %s\n", path);
		return;
	}

	char buf[2048];
	int n;
	while ((n = getdents64(fd, buf, sizeof(buf))) > 0) {
		char *p = buf;
		while (p < buf + n) {
			struct linux_dirent64 *d = (struct linux_dirent64 *) p;
			if (strcmp(d->d_name, ".") != 0 &&
			    strcmp(d->d_name, "..") != 0) {
				printf("%s\n", d->d_name);
			}
			p += d->d_reclen;
		}
	}

	close(fd);
}

void _start(int argc, char **argv)
{
	if (argc == 1) {
		list_dir(".");
		exit(0);
	}

	for (int i = 1; i < argc; i++)
		list_dir(argv[i]);

	exit(0);
}
