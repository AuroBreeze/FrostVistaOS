#ifndef __PLATFORM_DEFS_H__
#define __PLATFORM_DEFS_H__

#include "kernel/types.h"

// PLIC.c
void plic_init_uart(void);
void plic_set_priority(int id, int priority);
int plic_enable_interrupt(int context, int id);
int plic_set_threshold(int context, int threshold);
int plic_claim_interrupt(int context);
int plic_complete_interrupt(int context, int id);

// timer.c
void pre_timerinit();
void timerinit();

// sbi.c
void sbi_set_timer(uint64 stime_value);

// uart.c
void pre_uart_init();
void uartintr();
void uart_init();

#endif
