#ifndef __LOONGARCH_MACHINE_H
#define __LOONGARCH_MACHINE_H

#define DMW0_BASE 0x8000000000000000ULL // DMW0，可缓存普通内存
#define DMW1_BASE 0x9000000000000000ULL // DMW1，非缓存设备内存

#define DRAM_BASE_LOW 0x00200000ULL
#define DRAM_SIZE (126ULL * 1024 * 1024)
#define PHYSTOP_LOW (DRAM_BASE_LOW + DRAM_SIZE)
#define PHYSTOP_HIGH (DMW0_BASE | PHYSTOP_LOW)

#define PA2VA(pa) ((uint64) (pa) | DMW0_BASE)
#define VA2PA(va) ((uint64) (va) & 0x0000ffffffffffffULL)

#define DMW_VSEG_MASK 0xf000000000000000ULL
#define IS_DMW0_ADDR(va) (((uint64) (va) & DMW_VSEG_MASK) == DMW0_BASE)
#define IS_DMW1_ADDR(va) (((uint64) (va) & DMW_VSEG_MASK) == DMW1_BASE)

#define IS_RAM_KVA(va)                                                         \
	(IS_DMW0_ADDR(va) && (uint64) (va) >= (DMW0_BASE | DRAM_BASE_LOW) &&   \
	 (uint64) (va) < PHYSTOP_HIGH)

#endif
