#ifndef MACHINE_H
#define MACHINE_H

#include "kernel/types.h"

#define KERNEL_BASE 0x80000000UL
#define PHYSTOP (KERNEL_BASE + 128 * 1024 * 1024)

#define KERNEL_VIRT_OFFSET 0xFFFFFFC000000000UL // Hight Address Offset

#define ADR2HIGHT(adr)                                                         \
  ((uint64)(adr) +                                                             \
   (uint64)(KERNEL_VIRT_OFFSET)) // Lower Address to Hight Address

#define ADR2LOW(adr) ((uint64)(adr) - (uint64)(KERNEL_VIRT_OFFSET))

#define IS_ADR_HIGHT(adr) ((uint64)(adr) >= (uint64)KERNEL_VIRT_OFFSET)
#define IS_ADR_LOW(adr)                                                        \
  (((uint64)(adr) >= KERNEL_BASE) && ((uint64)(adr) <= PHYSTOP))

#define PGSIZE 4096

// Fixed number of pages allocated before pagination is enabled
#define EPAGES 256

#endif
