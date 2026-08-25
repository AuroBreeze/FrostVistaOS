#ifndef __LOONGARCH_TRAP_H
#define __LOONGARCH_TRAP_H

#include "kernel/types.h"

#define ECFG_VS_SHIFT 16
#define ECFG_VS(x) ((x) << ECFG_VS_SHIFT)

#define is_interrupt(x) (((x) >> ECFG_VS_SHIFT) == 1)

struct trapframe {
	uint64 zero;
	uint64 ra;
	uint64 tp;
	uint64 sp;

	uint64 a0;
	uint64 a1;
	uint64 a2;
	uint64 a3;
	uint64 a4;
	uint64 a5;
	uint64 a6;
	uint64 a7;

	uint64 t0;
	uint64 t1;
	uint64 t2;
	uint64 t3;
	uint64 t4;
	uint64 t5;
	uint64 t6;
	uint64 t7;
	uint64 t8;

	uint64 u0;

	uint64 fp;

	uint64 s0;
	uint64 s1;
	uint64 s2;
	uint64 s3;
	uint64 s4;
	uint64 s5;
	uint64 s6;
	uint64 s7;
	uint64 s8;
};
#endif
