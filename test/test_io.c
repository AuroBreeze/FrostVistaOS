// test sys_close and sys_dup
#include "user.h"

void _start()
{
	printf("--- Starting Dup & Close Test ---\n");

	int fd1 = open("/dev/tty", 2); // O_RDWR = 2
	if (fd1 < 0) {
		printf("Test failed: Could not open /dev/tty\n");
		exit(1);
	}

	int fd2 = dup(fd1);
	if (fd2 < 0) {
		printf("Test failed: dup(fd1) failed\n");
		exit(1);
	}
	printf("Success: Opened fd %d, duped to fd %d\n", fd1, fd2);

	// Verify that both FDs are working
	write(fd1, "Message from fd1\n", 17);
	write(fd2, "Message from fd2 (duped)\n", 25);

	// Verify reference counting: Close fd1; fd2 should still be writable.
	close(fd1);
	printf("Closed fd %d, trying to write to fd %d...\n", fd1, fd2);
	if (write(fd2, "Still working after close!\n", 27) < 0) {
		printf("Error: duped fd failed after original closed\n");
	} else {
		printf("Success: Duped fd is persistent.\n");
	}

	// Verify the FD recycling logic (FrostVistaOS should assign the lowest
	// available ID)
	close(fd2);
	int fd3 = open("/dev/tty", 2);
	printf("After closing all, new open returned fd: %d\n", fd3);

	if (fd3 == fd1 || fd3 == fd2) {
		printf("Success: FD %d was correctly recycled.\n", fd3);
	} else {
		printf("Note: FD recycled to %d (Expected %d or %d)\n", fd3,
		       fd1, fd2);
	}

	printf("--- Dup & Close Test Finished ---\n");
	exit(0);
}
