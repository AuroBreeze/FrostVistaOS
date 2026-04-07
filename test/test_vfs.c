#include "user.h"

#define O_WRONLY 0x001

void _start() {
  printf("VFS Test: Attempting to open /dev/tty...\n");

  // 1. Test by turning on the device
  int fd = open("/dev/tty", O_WRONLY);

  if (fd < 0) {
    printf("VFS Test: Open /dev/tty failed! (fd=%d)\n", fd);
    exit(1);
  }

  printf("VFS Test: Open success, got fd=%d\n", fd);

  // 2. Test writing data via FD
  char *msg = ">>> [VFS Success]: Data written via VFS to UART! <<<\n";
  long bytes = write(fd, msg, strlen(msg));

  if (bytes > 0) {
    printf("VFS Test: Successfully wrote %d bytes.\n", (int)bytes);
  } else {
    printf("VFS Test: Write failed!\n");
  }

  exit(0);
}
