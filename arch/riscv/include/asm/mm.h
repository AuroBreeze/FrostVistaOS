#ifndef MM_H
#define MM_H

#include "asm/machine.h"

// dirty bit that to use write back
#define PTE_D (1 << 7)
// Whether to visit
#define PTE_A (1 << 6)

#define PTE_G (1 << 5)
// U mode can access
#define PTE_U (1 << 4)
// allow perfom
#define PTE_X (1 << 3)
// allow write
#define PTE_W (1 << 2)
// allow read
#define PTE_R (1 << 1)
// present valid
#define PTE_V (1 << 0)

// Copy on write
#define PTE_COW (1 << 8)

#define VPN_MASK 0x1ff // Obtain the required VPN subnet mask
#define VPN_BITS 9     // Number of positions occupied by VA VPN
#define ADDR_PF 12 // Page Offset Between Virtual Address and Physical Address

// Get VPN for VA
#define VPN_GET(va, i)                                                         \
	(((uint64) (va) >> (ADDR_PF + (VPN_BITS * (i)))) & VPN_MASK)

#define PTE2PA(pte) (((pte) >> 10) << ADDR_PF)
#define PA2PTE(pa) (((uint64) (pa) >> ADDR_PF) << 10)
#define PTE_FLAGS(pte) ((pte) & 0x3ff)

#define PA2VA(adr)                                                             \
	((uint64) (adr) +                                                      \
	 (uint64) (KERNEL_VIRT_OFFSET)) // Lower Address to Hight Address

#define VA2PA(adr) ((uint64) (adr) - (uint64) (KERNEL_VIRT_OFFSET))

/* 通用内核代码中的 KVA 约定为直接映射的内核虚拟地址。 */
#define ARCH_PA2KVA(pa) PA2VA(pa)
#define ARCH_KVA2PA(va) VA2PA(va)

#define IS_ADR_HIGH(adr) ((uint64) (adr) >= (uint64) KERNEL_VIRT_OFFSET)
#define IS_ADR_LOW(adr)                                                        \
	(((uint64) (adr) >= KERNEL_BASE_LOW) && ((uint64) (adr) <= PHYSTOP_LOW))

#define IS_RAM_PA(pa)                                                          \
	((uint64) (pa) >= KERNEL_BASE_LOW && (uint64) (pa) <= PHYSTOP_LOW)

#define IS_RAM_KVA(va)                                                         \
	((uint64) (va) >= KERNEL_BASE_HIGH && (uint64) (va) <= PHYSTOP_HIGH)

#define PGSIZE 4096
#define PGROUNDUP(x) (((x) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(x) ((x) & ~(PGSIZE - 1))
#define MAKE_SATP(pagetable) ((8L << 60) | ((uint64) (pagetable) >> 12))

extern char _kernel_end[];
#define KERNEL_END ((uint64) _kernel_end)

#endif
