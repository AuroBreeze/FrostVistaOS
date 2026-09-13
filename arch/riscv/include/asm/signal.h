#ifndef FV_RISCV_INCLUDE_ASM_SIGNAL_H
#define FV_RISCV_INCLUDE_ASM_SIGNAL_H

#include "kernel/arch/types.h"

// RISC-V signal frame used to restore the interrupted user context.
struct sigframe {
	struct arch_trapframe saved_tf;
	uint64 saved_mask;
};

#endif
