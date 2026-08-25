#ifndef __ARCH_MACHINE_H
#define __ARCH_MACHINE_H

#include "kernel/types.h"
#include "platform/timer.h"

static inline uint64 arch_read_time()
{
	return r_time();
}

#endif
