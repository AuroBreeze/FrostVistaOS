#ifndef __KERNEL_ARCH_TRAP_H
#define __KERNEL_ARCH_TRAP_H

#include "kernel/types.h"

/* Configure the kernel stack used by the next user-mode trap entry. */
void set_kernel_stack(uint64 stack_top);

static inline void arch_set_kernel_stack(uint64 stack_top)
{
	set_kernel_stack(stack_top);
}

#endif
