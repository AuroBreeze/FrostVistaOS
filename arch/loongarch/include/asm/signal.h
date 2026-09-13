#ifndef FV_LOONGARCH_SIGNAL_H
#define FV_LOONGARCH_SIGNAL_H

#include "kernel/arch/types.h"

// LoongArch signal frame used to restore the interrupted user context.
struct sigframe {
	struct arch_trapframe saved_tf;
	uint64 saved_mask;
};

#endif
