#ifndef __KERNEL_ARCH_CPU_H
#define __KERNEL_ARCH_CPU_H

#include "asm/cpu.h"

static inline void arch_cpu_wait()
{
	cpu_wait();
}

#endif
