#include "user.h"

static inline long syscall(long num, long a0, long a1, long a2) {
  register long a0_asm __asm__("a0") = a0;
  register long a1_asm __asm__("a1") = a1;
  register long a2_asm __asm__("a2") = a2;
  register long a7_asm __asm__("a7") = num;
  __asm__ volatile("ecall"
                   : "+r"(a0_asm)
                   : "r"(a1_asm), "r"(a2_asm), "r"(a7_asm)
                   : "memory");
  return a0_asm;
}

long write(int fd, const char *buf, uint64 count) {
  return syscall(SYS_write, fd, (long)buf, count);
}
void exit(int status) {
  syscall(SYS_exit, status, 0, 0);
  while (1)
    ;
}
void *sbrk(int increment) { return (void *)syscall(SYS_sbrk, increment, 0, 0); }
int fork(void) { return syscall(SYS_fork, 0, 0, 0); }
int wait(void) { return syscall(SYS_wait, 0, 0, 0); }
void print(const char *str) { write(1, str, strlen(str)); }

void print_int(int num) {
  char buf[16];
  int i = 0;
  if (num == 0) {
    print("0");
    return;
  }
  while (num > 0) {
    buf[i++] = (num % 10) + '0';
    num /= 10;
  }
  // Print the digits in reverse order (correct to left-to-right)
  while (i > 0) {
    i--;
    write(1, &buf[i], 1);
  }
}
unsigned long strlen(const char *s) {
  unsigned long n = 0;
  while (s[n])
    n++;
  return n;
}
