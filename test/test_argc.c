#include "user.h"
// According to RISC-V calling conventions, the compiler will
// automatically fetch 'argc' from 'a0' and 'argv' from 'a1'.
void _start(int argc, char *argv[]) {
  printf("Hello FrostVista OS!\n");
  printf("Let's check the arguments passed by exec:\n");

  printf("argc: %d\n", argc);

  for (int i = 0; i < argc; i++) {
    printf("argv[%d]: %s\n", i, argv[i]);
    // Print the strings you successfully pushed onto the user stack via
    // copyout!
    printf(argv[i]);
    printf("\n");
  }

  printf("Init process is exiting cleanly...\n");
  exit(0);
}
