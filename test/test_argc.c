#include "user.h"
// According to RISC-V calling conventions, the compiler will
// automatically fetch 'argc' from 'a0' and 'argv' from 'a1'.
void _start(int argc, char *argv[]) {
  print("Hello FrostVista OS!\n");
  print("Let's check the arguments passed by exec:\n");

  print("argc: ");
  print_int(argc);
  print("\n");

  for (int i = 0; i < argc; i++) {
    print("argv[");
    print_int(i);
    print("]: ");
    // Print the strings you successfully pushed onto the user stack via
    // copyout!
    print(argv[i]);
    print("\n");
  }

  print("Init process is exiting cleanly...\n");
  exit(0);
}
