#ifndef ARCH_RISCV_SIGNAL_H
#define ARCH_RISCV_SIGNAL_H

#include "core/proc.h"

// RISC-V signal frame used to restore the interrupted user context.
struct sigframe {
	struct trapframe saved_tf;
	uint64 saved_mask;
};

#endif
