#ifndef __KERNEL_ARCH_MM_H
#define __KERNEL_ARCH_MM_H

#include "kernel/types.h"
#include "asm/mm.h"

static inline uint64 arch_pa_to_kva(uint64 pa)
{
	return PA2VA(pa);
}

static inline uint64 arch_kva_to_pa(uint64 va)
{
	return VA2PA(va);
}

static inline int arch_is_ram_pa(uint64 pa)
{
	return IS_RAM_PA(pa);
}

static inline int arch_is_ram_kva(uint64 va)
{
	return IS_RAM_KVA(va);
}

#endif
