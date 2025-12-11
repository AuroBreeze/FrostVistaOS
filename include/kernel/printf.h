#ifndef PRINTF_H
#define PRINTF_H

void kputc(char c);
void kputs(const char *s);
void kprintf(const char *fmt, ...);
void panic(const char *s);

#endif
