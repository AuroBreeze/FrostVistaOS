#ifndef FV_KERNEL_ARCH_TIMER_H
#define FV_KERNEL_ARCH_TIMER_H

#include "kernel/types.h"
#include "platform/timer.h"

static inline uint64 arch_read_time()
{
	return r_time();
}

#endif
