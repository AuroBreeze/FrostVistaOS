#include "kernel/printf.h"
#include "driver/uart.h"
#include <stdarg.h>
#include <stdint.h>

static const char digits[] = "0123456789abcdef";

void kputc(char c) {
  // 如果你想兼容串口终端的换行，可以加：
  // if (c == '\n') uart_putc('\r');
  uart_putc(c);
}

void kputs(const char *s) {
  while (*s) {
    kputc(*s++);
  }
}

static void kprintint(long long xx, int base, int sign) {
  char buf[32];
  int i = 0;
  unsigned long long x;

  if (sign && xx < 0) {
    x = -xx;
  } else {
    x = xx;
  }

  if (x == 0) {
    buf[i++] = '0';
  } else {
    while (x != 0) {
      buf[i++] = digits[x % base];
      x /= base;
    }
  }

  if (sign && xx < 0)
    buf[i++] = '-';

  while (--i >= 0)
    kputc(buf[i]);
}

static void kprintptr(uint64_t x) {
  kputs("0x");
  for (int i = 0; i < 16; i++, x <<= 4) {
    kputc(digits[(x >> 60) & 0xf]);
  }
}

void kprintf(const char *fmt, ...) {
  if (!fmt) {
    kputs("kprintf: NULL fmt\n");
    while (1) {
    }
  }

  va_list ap;
  va_start(ap, fmt);

  for (int i = 0; fmt[i] != '\0'; i++) {
    if (fmt[i] != '%') {
      kputc(fmt[i]);
      continue;
    }

    // 处理格式串
    char c = fmt[++i];
    if (c == '\0')
      break;

    switch (c) {
    case 'd':
      kprintint(va_arg(ap, int), 10, 1);
      break;
    case 'x':
      kprintint(va_arg(ap, unsigned int), 16, 0);
      break;
    case 'p':
      kprintptr(va_arg(ap, uint64_t));
      break;
    case 's': {
      const char *s = va_arg(ap, const char *);
      if (!s)
        s = "(null)";
      kputs(s);
      break;
    }
    case 'c': {
      int ch = va_arg(ap, int);
      kputc((char)ch);
      break;
    }
    case '%':
      kputc('%');
      break;
    default:
      kputc('%');
      kputc(c);
      break;
    }
  }

  va_end(ap);
}

__attribute__((noreturn)) 
void panic(const char *s) {
  kprintf("panic: %s\n", s);
  while (1) {
  }
}
