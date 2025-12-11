#ifndef PRINTF_H
#define PRINTF_H

#include <stdnoreturn.h>
void kputc(char c);
void kputs(const char *s);
void kprintf(const char *fmt, ...);
void panic(const char *s);

#endif
