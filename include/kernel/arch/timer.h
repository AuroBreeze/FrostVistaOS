#ifndef FV_KERNEL_ARCH_TIMER_H
#define FV_KERNEL_ARCH_TIMER_H

#include "kernel/types.h"
#include "platform/timer.h"

/* Raw hardware ticks; use these only with the architecture's TIMER_FREQ. */
static inline uint64 arch_read_time(void)
{
	return r_time();
}

/* Both supported platforms have an integral number of ticks per microsecond. */
static inline uint64 arch_read_time_us(void)
{
	return arch_read_time() / (TIMER_FREQ / 1000000ULL);
}

#endif
