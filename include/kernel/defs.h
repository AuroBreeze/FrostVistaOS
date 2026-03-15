#ifndef DEFS_H
#define DEFS_H

#include "kernel/types.h"

// proc.c
int cpuid();
void user_mode_start();
void usertrap(uint64 *);

// kalloc.c
void kalloc_init();
void kfree(void *pa);
void *kalloc();
void *ekalloc();

// printf.c
void kputc(char c);
void kputs(const char *s);
void kprintf(const char *fmt, ...);
void _panic(const char *file, int line, const char *fmt, ...);

// string.c
void *memset(void *s, int c, long n);
void *memcpy(void *dest, const void *src, long n);
void *memmove(void *dest, const void *src, long n);
long strlen(const char *str);

#endif
