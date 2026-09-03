#ifndef __RISCV_PLATFORM_TIMER_H
#define __RISCV_PLATFORM_TIMER_H

#include "kernel/types.h"

static inline uint64 r_time()
{
	uint64 x;
	// csrr: Control Status Register Read
	asm volatile("csrr %0, time" : "=r"(x));
	return x;
}

#endif
