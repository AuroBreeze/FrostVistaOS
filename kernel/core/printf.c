#include "driver/hal_console.h"
#include "kernel/defs.h"
#include "kernel/types.h"
#include <stdarg.h>

static const char digits[] = "0123456789abcdef";

void kputc(char c) { hal_console_putc(c); }

void kputs(const char *s) {
  while (*s) {
    kputc(*s++);
  }
}

static void kprintint(long long xx, int base, int sign) {
  char buf[32];
  int i = 0;
  uint64 x;

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

static void kprintptr(uint64 x) {
  kputs("0x");
  for (int i = 0; i < 16; i++, x <<= 4) {
    kputc(digits[(x >> 60) & 0xf]);
  }
}

void vkprintf(const char *fmt, va_list ap) {
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
      kprintint(va_arg(ap, uint), 16, 0);
      break;
    case 'p':
      kprintptr(va_arg(ap, uint64));
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
}

void kprintf(const char *fmt, ...) {
  if (!fmt) {
    kputs("kprintf: NULL fmt\n");
    while (1) {
    }
  }

  va_list ap;
  va_start(ap, fmt);
  vkprintf(fmt, ap);
  va_end(ap);
}

void _panic(const char *file, int line, const char *fmt, ...) {
  kprintf("\033[1;31m[KERNEL PANIC] at %s:%d\nReason: ", file, line);

  va_list ap;
  va_start(ap, fmt);
  vkprintf(fmt, ap);
  // LOG_ERROR(fmt, ap);
  va_end(ap);
  kputs("\033[0m\n");

  while (1) {
    __asm__ volatile("wfi");
  }
}
