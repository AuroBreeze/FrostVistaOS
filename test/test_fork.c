#include "user.h"

void _start() {
  print("--- Testing fork() ---\n");

  int shared_var = 100;
  print("[Main] Initial shared_var = ");
  print_int(shared_var);
  print("\n");

  int pid = fork();

  if (pid < 0) {
    print("[Error] fork failed!\n");
    exit(1);
  } else if (pid == 0) {
    print("[Child] Hello from child process!\n");

    shared_var += 50;
    print("[Child] I modified shared_var to: ");
    print_int(shared_var);
    print("\n");

    print("[Child] Exiting...\n");
    exit(0);
  } else {
    print("[Parent] Hello from parent process!\n");
    print("[Parent] Created child with PID: ");
    print_int(pid);
    print("\n");

    print("[Parent] Waiting for child to exit...\n");

    int reaped_pid = wait();

    print("[Parent] Child ");
    print_int(reaped_pid);
    print(" has been reaped.\n");

    print("[Parent] My shared_var is still: ");
    print_int(shared_var);
    print(" (should be 100)\n");

    if (shared_var == 100) {
      print("--- fork() memory isolation test PASSED ---\n");
    } else {
      print("--- fork() memory isolation test FAILED ---\n");
    }

    exit(0);
  }
}
