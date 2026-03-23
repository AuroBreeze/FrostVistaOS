#include "user.h"

void _start() {
  int id = 0; // 0 for parent, > 0 for child logical ID
  int num_children = 3; // Number of child processes to create

  printf("Scheduler test starting...\n"); //

  // Loop to create multiple child processes
  for (int i = 1; i <= num_children; i++) {
    int pid = fork(); //
    if (pid < 0) {
      printf("Fork failed!\n"); //
      exit(1); //
    }
    
    if (pid == 0) {
      // Child process branch
      id = i; 
      break; // Exit loop to prevent children from forking their own children
    }
  }

  // Both parent and 3 children reach this loop
  volatile int count = 0;
  while (count < 5) {
    if (id == 0) {
      printf("Parent running, count = %d\n", count); //
    } else {
      printf("Child %d running, count = %d\n", id, count); //
    }

    // Artificial delay to consume CPU time.
    // This increases the chance of a timer interrupt triggering 
    // a context switch during the process's time slice.
    volatile int delay = 0;
    while (delay < 500000) {
        delay++;
    }

    count++;
  }

  // Final synchronization
  if (id == 0) {
    // Parent waits for all children to exit to clean up Zombies
    for (int i = 0; i < num_children; i++) {
      wait(); //
    }
    printf("Parent: All children finished. Test done.\n"); //
  }

  exit(0); //
}
