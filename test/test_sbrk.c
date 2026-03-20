// test_sbrk.c - Specialized test for heap allocation
#include "user.h"

void _start() {
  print("--- Testing sbrk growth ---\n");
  char *old_brk = sbrk(0);
  char *new_mem = sbrk(4096);

  if (new_mem == old_brk) {
    print("Success: Allocated 4KB.\n");
    new_mem[0] = 'A'; // Verifying write access
  }

  print("Test finished.\n");
  exit(0);
}
