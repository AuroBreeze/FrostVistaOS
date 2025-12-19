#ifndef DEFS_H
#define DEFS_H

#include "kernel/types.h"
#include <stddef.h>

// uart.c
void uart_init();
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc();

// kalloc.c
void kalloc_init();
void kfree(void *pa);
void *kalloc();

// vm.c
void kvminithart();
void kvminit();
int mappages(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm);
int kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm);

// printf.c
void kputc(char c);
void kputs(const char *s);
void kprintf(const char *fmt, ...);
void panic(const char *s);

// string.c
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
size_t strlen(const char *str);

// tool.c
uint64 next_pc();

#endif
