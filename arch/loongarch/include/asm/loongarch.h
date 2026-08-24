#ifndef __LOONGARCH_H__
#define __LOONGARCH_H__

#include "kernel/types.h"

static inline void w_eentry(uint64 x)
{
	asm volatile("csrwr %0, 0xc" : "+r"(x));
}

static inline void w_ecfg(uint64 x)
{
	asm volatile("csrwr %0, 0x4" : "+r"(x));
}

#endif
