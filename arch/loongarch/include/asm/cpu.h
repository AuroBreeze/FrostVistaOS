#ifndef __LOONGARCH_ASM_CPU_H
#define __LOONGARCH_ASM_CPU_H

static inline void cpu_wait()
{
	asm volatile("idle 0");
}

#endif
