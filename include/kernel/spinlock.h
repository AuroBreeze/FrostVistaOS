#ifndef FV_KERNEL_SPINLOCK_H
#define FV_KERNEL_SPINLOCK_H

#include "kernel/types.h"

struct spinlock {
	uint locked;
	char *name;
	struct cpu *cpu;
};

#endif
