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

static inline uint64 r_estat()
{
	uint64 x;
	asm volatile("csrrd %0, 0x5" : "=r"(x));
	return x;
}

static inline uint64 r_badv()
{
	uint64 x;
	asm volatile("csrrd %0, 0x7" : "=r"(x));
	return x;
}

static inline uint64 r_badi()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8" : "=r"(x));
	return x;
}

#endif
