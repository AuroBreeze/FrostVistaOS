#ifndef FV_RISCV_TLB_H
#define FV_RISCV_TLB_H

#include "asm/riscv.h"

static inline void hal_tlb_flush_all(void)
{
	sfence_vma();
}

#endif
