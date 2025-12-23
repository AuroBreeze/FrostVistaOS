#ifndef MACHINE_H
#define MACHINE_H

#include "kernel/types.h"

#define KERNEL_VIRT_OFFSET 0xFFFFFFC000000000UL // Hight Address Offset
#define KERNEL_BASE_LOW 0x80000000UL
#define PHYSTOP_LOW (KERNEL_BASE_LOW + 128 * 1024 * 1024)
#define KERNEL_BASE_HIGH (KERNEL_BASE_LOW + KERNEL_VIRT_OFFSET)
#define PHYSTOP_HIGH (PHYSTOP_LOW + KERNEL_VIRT_OFFSET)

#define ADR2HIGHT(adr)                                                         \
  ((uint64)(adr) +                                                             \
   (uint64)(KERNEL_VIRT_OFFSET)) // Lower Address to Hight Address

#define ADR2LOW(adr) ((uint64)(adr) - (uint64)(KERNEL_VIRT_OFFSET))

#define IS_ADR_HIGHT(adr) ((uint64)(adr) >= (uint64)KERNEL_VIRT_OFFSET)
#define IS_ADR_LOW(adr)                                                        \
  (((uint64)(adr) >= KERNEL_BASE_LOW) && ((uint64)(adr) <= PHYSTOP_LOW))

#define PGSIZE 4096
#define NCPU 16

#endif
