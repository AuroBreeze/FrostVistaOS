#ifndef FV_LOONGARCH_MACHINE_H
#define FV_LOONGARCH_MACHINE_H

#define DMW0_BASE 0x8000000000000000ULL // DMW0, cacheable normal memory
#define DMW1_BASE 0x9000000000000000ULL // DMW1, uncached device memory

/* LoongArch64 address width and the final kernel high-half offset. */
#define LOONGARCH_PALEN 48
#define LOONGARCH_PA_MASK ((1ULL << LOONGARCH_PALEN) - 1ULL)
#define KERNEL_VIRT_OFFSET 0xffffffc000000000ULL
#define KERNEL_IO_BASE 0xffffffd000000000ULL

#define DRAM_BASE_LOW 0x00200000ULL
#define DRAM_SIZE (126ULL * 1024 * 1024)
#define PHYSTOP_LOW (DRAM_BASE_LOW + DRAM_SIZE)
#define DMW0_PHYSTOP_HIGH (DMW0_BASE | PHYSTOP_LOW)
#define PHYSTOP_HIGH (KERNEL_VIRT_OFFSET + PHYSTOP_LOW)

/* Maximum virtual address available to user space. */
#define USER_VA_TOP (1ULL << 38)

#endif
