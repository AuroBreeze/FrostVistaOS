#ifndef __LOONGARCH_MM_H
#define __LOONGARCH_MM_H

#include "asm/machine.h"

#define PGSIZE (4096)

#define DMW_VSEG_MASK 0xf000000000000000ULL
#define IS_DMW0_ADDR(va) (((uint64) (va) & DMW_VSEG_MASK) == DMW0_BASE)
#define IS_DMW1_ADDR(va) (((uint64) (va) & DMW_VSEG_MASK) == DMW1_BASE)

#define IS_RAM_PA(pa)                                                          \
	((uint64) (pa) >= DRAM_BASE_LOW && (uint64) (pa) < PHYSTOP_LOW)

#define IS_RAM_KVA(va)                                                         \
	(IS_DMW0_ADDR(va) && (uint64) (va) >= (DMW0_BASE | DRAM_BASE_LOW) &&   \
	 (uint64) (va) < PHYSTOP_HIGH)

#define PA2VA(pa) ((uint64) (pa) | DMW0_BASE)
#define VA2PA(va) ((uint64) (va) & 0x0000ffffffffffffULL)

#endif
