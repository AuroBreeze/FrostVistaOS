#include "user.h"
#include <stdarg.h>

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
int open(const char *path, int flags) {
  return (int)syscall(SYS_open, (long)path, flags, 0);
}
long read(int fd, void *buf, uint64 count) {
  return syscall(SYS_read, fd, (long)buf, count);
}

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

void printf(const char *fmt, ...) {
  char buf[256];
  int pos = 0;
  va_list ap;
  va_start(ap, fmt);

  for (int i = 0; fmt[i] != '\0' && pos < 250; i++) {
    if (fmt[i] != '%') {
      buf[pos++] = fmt[i];
      continue;
    }

    i++;
    if (fmt[i] == 'd') {
      int num = va_arg(ap, int);
      char tmp[16];
      int tpos = 0;
      if (num < 0) {
        buf[pos++] = '-';
        num = -num;
      }
      if (num == 0)
        tmp[tpos++] = '0';
      while (num > 0) {
        tmp[tpos++] = (num % 10) + '0';
        num /= 10;
      }
      while (tpos > 0 && pos < 250) {
        buf[pos++] = tmp[--tpos];
      }
    } else if (fmt[i] == 's') {
      char *s = va_arg(ap, char *);
      while (*s && pos < 250) {
        buf[pos++] = *s++;
      }
    } else {
      buf[pos++] = '%';
      buf[pos++] = fmt[i];
    }
  }
  va_end(ap);

  write(1, buf, pos);
}
