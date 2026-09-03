#ifndef __LOONGARCH_ASM_CPU_H
#define __LOONGARCH_ASM_CPU_H

#include "asm/loongarch.h"

static inline int hal_get_cpu_id()
{
	return (int) r_cpuid();
}

static inline void cpu_wait()
{
	asm volatile("idle 0");
}

#endif
