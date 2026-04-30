#include "user.h"

void _start()
{
	printf("--- Testing fork() ---\n");

	int shared_var = 100;
	printf("[Main] Initial shared_var = %d\n", shared_var);

	int pid = fork();

	if (pid < 0) {
		printf("[Error] fork failed!\n");
		exit(1);
	} else if (pid == 0) {
		printf("[Child] Hello from child process!\n");

		shared_var += 50;
		printf("[Child] I modified shared_var to: %d\n", shared_var);

		printf("[Child] Exiting...\n");
		exit(0);
	} else {
		printf("[Parent] Hello from parent process!\n");
		printf("[Parent] Created child with PID: %d\n", pid);

		printf("[Parent] Waiting for child to exit...\n");

		int reaped_pid = wait();

		printf("[Parent] Child %d has been reaped.\n", reaped_pid);

		printf("[Parent] My shared_var is still: %d (should be 100)\n",
		       shared_var);

		if (shared_var == 100) {
			printf("--- fork() memory isolation test PASSED ---\n");
		} else {
			printf("--- fork() memory isolation test FAILED ---\n");
		}

		exit(0);
	}
}
