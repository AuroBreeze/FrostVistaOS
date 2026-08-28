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

static inline int arch_is_ram_pa(uint64 pa)
{
	return IS_RAM_PA(pa);
}

static inline int arch_is_ram_kva(uint64 va)
{
	return IS_RAM_KVA(va);
}

#define ARCH_KERNEL_END KERNEL_END

#define ARCH_DRAM_SIZE DRAM_SIZE
#define ARCH_PGSIZE PGSIZE
#define ARCH_DRAM_BASE_LOW DRAM_BASE_LOW
#define ARCH_PHYSTOP_HIGH PHYSTOP_HIGH

#endif
