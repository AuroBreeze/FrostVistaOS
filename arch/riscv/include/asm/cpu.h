#ifndef _ASM_CPU_H
#define _ASM_CPU_H

#include "asm/riscv.h"
static inline int hal_get_cpu_id()
{
	int cpuid = r_tp();
	return cpuid;
}

#endif
