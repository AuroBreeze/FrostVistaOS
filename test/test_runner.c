#include "user.h"

void _start(void)
{
	printf("#### OS COMP TEST GROUP START basic-musl ####\n");

	int pid = fork();
	if (pid == 0) {
		exec("/musl/basic/getpid");
		exit(1);
	}

	wait();
	printf("#### OS COMP TEST GROUP END basic-musl ####\n");
	shutdown();
}
