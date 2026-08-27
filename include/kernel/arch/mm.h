#ifndef __KERNEL_ARCH_MM_H
#define __KERNEL_ARCH_MM_H

#include "kernel/types.h"
#include "asm/mm.h"

static inline uint64 arch_pa_to_kva(uint64 pa)
{
	/* 正式高半区内核映射，仅能用于已经建立映射的物理页。 */
	return ARCH_PA2KVA(pa);
}

static inline uint64 arch_kva_to_pa(uint64 va)
{
	/* 正式高半区内核虚拟地址。 */
	return ARCH_KVA2PA(va);
}

static inline uint64 arch_pa_to_direct_va(uint64 pa)
{
	return ARCH_PA2DIRECT_VA(pa);
}

static inline uint64 arch_direct_va_to_pa(uint64 va)
{
	return ARCH_DIRECT_VA2PA(va);
}

static inline int arch_is_ram_pa(uint64 pa)
{
	return IS_RAM_PA(pa);
}

static inline int arch_is_direct_ram_va(uint64 va)
{
	return ARCH_IS_DIRECT_RAM_VA(va);
}

#endif
