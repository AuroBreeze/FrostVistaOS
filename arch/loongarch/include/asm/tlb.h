#ifndef FV_LOONGARCH_TLB_H
#define FV_LOONGARCH_TLB_H

#include "asm/loongarch.h"

LA_ALWAYS_INLINE void hal_tlb_flush_all(void)
{
	invtlb_all();
}

#endif
