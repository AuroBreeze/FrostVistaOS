#ifndef __KERNEL_ARCH_TRAP_H
#define __KERNEL_ARCH_TRAP_H

#include "kernel/types.h"

/* Configure the kernel stack used by the next user-mode trap entry. */
void set_kernel_stack(uint64 stack_top);

/* Dispatch a trap after the architecture entry code saved its context. */
void arch_trap_handle(void);

/* Return from the kernel to the current user process. */
void arch_usertrapret(void);

static inline void arch_set_kernel_stack(uint64 stack_top)
{
	set_kernel_stack(stack_top);
}

#endif
