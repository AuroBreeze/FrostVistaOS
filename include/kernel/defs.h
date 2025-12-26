#ifndef DEFS_H
#define DEFS_H

#include "kernel/types.h"

// sbi.c
void sbi_set_timer(uint64 stime_value);
void timer_handle();

// trap.c
void trapinit();

// timer.c
void pre_timerinit();
void timerinit();

// uart.c
void pre_uart_init();
void uartintr();
void uart_init();
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc();

// PLIC.c
void plic_init_uart(void);
void plic_set_priority(int id, int priority);
int plic_enable_interrupt(int context, int id);
int plic_set_threshold(int context, int threshold);
int plic_claim_interrupt(int context);
int plic_complete_interrupt(int context, int id);

// proc.c
int cupid();

// kalloc.c
void kalloc_init();
void kfree(void *pa);
void *kalloc();
void *ekalloc();

// vm.c
void kvminithart();
void kvminit();
int mappages(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm);
int kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm);
uint64 walk_addr(pagetable_t pagetable, uint64 va);
void kvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free);

// printf.c
void kputc(char c);
void kputs(const char *s);
void kprintf(const char *fmt, ...);
void panic(const char *s);

// string.c
void *memset(void *s, int c, long n);
void *memcpy(void *dest, const void *src, long n);
void *memmove(void *dest, const void *src, long n);
long strlen(const char *str);

// tool.c
uint64 next_pc();
uint64 pt_alloc_page_pa();
#endif
