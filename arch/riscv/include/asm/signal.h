#ifndef ARCH_RISCV_SIGNAL_H
#define ARCH_RISCV_SIGNAL_H

#include "kernel/arch/types.h"

// RISC-V signal frame used to restore the interrupted user context.
struct sigframe {
	struct arch_trapframe saved_tf;
	uint64 saved_mask;
};

#endif
