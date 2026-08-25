#ifndef __LOONGARCH_CONTEXT_H
#define __LOONGARCH_CONTEXT_H

#include "kernel/types.h"

// Kernel context layout reserved for the LoongArch scheduler path.
struct arch_context {
	uint64 ra;
	uint64 sp;

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
