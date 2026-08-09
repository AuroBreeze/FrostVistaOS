#include "user.h"
#include "libtest.h"

#define NBLK 256
#define MAXR 10000

int main(void)
{
	char buf[512];
	int fd = 0;
	int n = 0;
	int rounds = 0;

	printf("readloop: starting\n");
	unlink("readfile");
	fd = open("readfile", O_CREATE | O_RDWR);
	if (fd < 0) {
		printf("readloop: create failed\n");
		exit(1);
	}
	memset(buf, 'a', sizeof(buf));
	for (int i = 0; i < NBLK; i++) {
		if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
			printf("readloop: write failed\n");
			exit(1);
		}
	}
	close(fd);
	printf("readloop: file ready (%d blocks)\n", NBLK);

	for (;;) {
		fd = open("readfile", 0);
		if (fd < 0) {
			printf("readloop: open failed\n");
			exit(1);
		}
		while ((n = read(fd, buf, sizeof(buf))) > 0)
			;
		close(fd);

		rounds++;
		if (rounds % 100 == 0)
			printf("readloop: round %d\n", rounds);
		if (rounds >= MAXR)
			break;
	}

	printf("readloop: done, total rounds=%d\n", rounds);
	exit(0);
}
