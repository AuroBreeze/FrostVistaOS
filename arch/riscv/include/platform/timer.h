#ifndef FV_RISCV_PLATFORM_TIMER_H
#define FV_RISCV_PLATFORM_TIMER_H

#include "kernel/types.h"

// QEMU virt's time counter runs at 10 MHz.
#define TIMER_FREQ 10000000ULL

static inline uint64 r_time()
{
	uint64 x;
	// csrr: Control Status Register Read
	asm volatile("csrr %0, time" : "=r"(x));
	return x;
}

#endif
