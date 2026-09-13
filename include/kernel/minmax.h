#ifndef KERNEL_MINMAX_H
#define KERNEL_MINMAX_H

#include "kernel/types.h"

static inline uint64 min_u64(uint64 a, uint64 b)
{
	return a < b ? a : b;
}

static inline uint64 max_u64(uint64 a, uint64 b)
{
	return a > b ? a : b;
}

#endif
