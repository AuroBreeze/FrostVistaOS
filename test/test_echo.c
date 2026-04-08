#include "user.h"

void _start() {
  char buf[10];
  printf("FrostVistaOS Shell Test. Please type something: \n");

  printf("> ");
  buf[9] = '\0';
  while (1) {
    //  Read from fd 0 (stdin/uart)
    int n = read(0, buf, sizeof(buf) - 1);

    if (n > 0) {
      buf[n] = '\0'; // Ensure the string ends
      printf("You typed (%d bytes): %s\n", n, buf);
      printf("\n%s\n", buf);

      // If you enter “exit”, the test will end.
      if (strlen(buf) >= 4 && buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'i' &&
          buf[3] == 't') {
        break;
      }
    } else if (n < 0) {
      printf("Read error!\n");
      break;
    }
  }

  printf("Echo test finished.\n");
  exit(0);
}
