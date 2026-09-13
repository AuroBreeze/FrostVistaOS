#ifndef FV_LOONGARCH_PLATFORM_TIMER_H
#define FV_LOONGARCH_PLATFORM_TIMER_H

#include "kernel/types.h"

// The QEMU LoongArch constant-frequency timer runs at 100 MHz.
#define TIMER_FREQ 100000000ULL
// Match the current RISC-V 100 ms scheduling period: 10 ticks per second.
#define TIMER_HZ 10ULL

// TCFG uses its low two bits for enable and periodic mode; the initial
// countdown value must be aligned to 4.
#define TCFG_INITVAL_MASK 0xfffffffffffcULL

// TCFG.EN[0]: setting this bit starts the countdown.
#define TCFG_ENABLE (1 << 0)
// TCFG.Periodic[1]: setting this bit reloads the initial value at zero.
#define TCFG_PERIODIE (1 << 1)
// Encode the constant-frequency tick period in TCFG.InitVal.
#define TCFG_INITVAL(ticks) ((uint64) (ticks) & TCFG_INITVAL_MASK)

static inline uint64 r_time()
{
	uint64 x;
	asm volatile("rdtime.d %0, $zero" : "=r"(x));
	return x;
}

void timer_init();

#endif
