#ifndef RIME_STDIO_H
#define RIME_STDIO_H

/* Minimal interface: %d, %x, %p, %s, %c, %%, and %lu. */
void printf(const char *fmt, ...);
void print(const char *str);

#endif
