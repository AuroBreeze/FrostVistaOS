#ifndef FV_LOONGARCH_MM_H
#define FV_LOONGARCH_MM_H

#include "asm/machine.h"
#include "kernel/macros.h"
#include "kernel/vm.h"
#include "kernel/types.h"

#define PGSIZE (4096)
#define PGROUNDUP(value) ALIGN_UP((value), PGSIZE)
#define PGROUNDDOWN(value) ALIGN_DOWN((value), PGSIZE)

/*
 * Page-table entry format for regular 4 KiB memory pages.
 *
 * This is the in-memory PTE format, not the TLBELO0/TLBELO1 format. The PTE
 * V/D bits occupy the same positions as their TLB counterparts, but the P/W
 * bits are at bits 7 and 8 and cannot be used directly as TLBELO fields.
 */
#define LA_PTE_V (1ULL << 0) /* Page-table entry is valid. */
#define LA_PTE_D (1ULL << 1) /* Page-table entry is dirty. */
#define LA_PTE_PLV_SHIFT 2
#define LA_PTE_PLV_MASK (3ULL << LA_PTE_PLV_SHIFT)
#define LA_PTE_MAT_SHIFT 4
#define LA_PTE_MAT_MASK (3ULL << LA_PTE_MAT_SHIFT)
#define LA_PTE_G (1ULL << 6)   /* Global mapping for a regular PTE. */
#define LA_PTE_H (1ULL << 6)   /* Huge-page directory flag; must remain zero. */
#define LA_PTE_P (1ULL << 7)   /* Physical page is present. */
#define LA_PTE_W (1ULL << 8)   /* Writes are permitted. */
#define LA_PTE_COW (1ULL << 9) /* Copy-on-write is enabled. */
#define LA_PTE_PPN_SHIFT 12
#define LA_PTE_PPN_MASK (LOONGARCH_PA_MASK & ~(PGSIZE - 1ULL))
#define LA_PTE_NR (1ULL << 61)	 /* Not readable. */
#define LA_PTE_NX (1ULL << 62)	 /* Not executable. */
#define LA_PTE_RPLV (1ULL << 63) /* Restrict privilege level. */

#define LA_PTE_VALID_MASK (LA_PTE_V | LA_PTE_P)
#define LA_PTE_IS_VALID(pte)                                                   \
	(((uint64) (pte) & LA_PTE_VALID_MASK) == LA_PTE_VALID_MASK)

#define LA_PTE_PLV0 (0ULL << LA_PTE_PLV_SHIFT)
#define LA_PTE_PLV3 (3ULL << LA_PTE_PLV_SHIFT)

#define LA_PTE_MAT_SUC (0ULL << LA_PTE_MAT_SHIFT) /* Strongly ordered uncached. */
#define LA_PTE_MAT_CC (1ULL << LA_PTE_MAT_SHIFT)  /* Coherent cached. */
#define LA_PTE_MAT_WUC (2ULL << LA_PTE_MAT_SHIFT) /* Weakly ordered uncached. */

#define LA_PTE_PA(pte) ((uint64) (pte) & LA_PTE_PPN_MASK)
#define LA_PA_PTE(pa) ((uint64) (pa) & LA_PTE_PPN_MASK)

/* TLBELO0/TLBELO1 format; the PTE P/W bits are not TLBELO fields. */
#define LA_TLB_V (1ULL << 0)
#define LA_TLB_D (1ULL << 1)
#define LA_TLB_PLV_SHIFT 2
#define LA_TLB_PLV_MASK (3ULL << LA_TLB_PLV_SHIFT)
#define LA_TLB_MAT_SHIFT 4
#define LA_TLB_MAT_MASK (3ULL << LA_TLB_MAT_SHIFT)
#define LA_TLB_G (1ULL << 6)
#define LA_TLB_PPN_MASK (LOONGARCH_PA_MASK & ~(PGSIZE - 1ULL))
#define LA_TLB_NR (1ULL << 61)
#define LA_TLB_NX (1ULL << 62)
#define LA_TLB_RPLV (1ULL << 63)

static inline uint64 pte_from_perm(uint64 perm)
{
	uint64 flags = (perm & PTE_USER) ? LA_PTE_PLV3 : LA_PTE_PLV0;

	/* LoongArch expresses read/execute permissions as negative bits. */
	if (!(perm & PTE_READ))
		flags |= LA_PTE_NR;
	if (!(perm & PTE_EXEC))
		flags |= LA_PTE_NX;
	if (perm & PTE_WRITE)
		flags |= LA_PTE_W | LA_PTE_D;

	return flags;
}

static uint64 loongarch_user_pte_flags(pte_t pte)
{
	return pte & (LA_PTE_D | LA_PTE_PLV_MASK | LA_PTE_MAT_MASK | LA_PTE_G |
		      LA_PTE_W | LA_PTE_NR | LA_PTE_NX | LA_PTE_RPLV |
		      LA_PTE_COW | LA_PTE_P | LA_PTE_V);
}

/* Convert a regular memory PTE to TLBELO0/TLBELO1 format. */
static inline __attribute__((always_inline)) uint64
loongarch_pte_to_tlbelo(pte_t pte)
{
	if (!LA_PTE_IS_VALID(pte))
		return 0;

	uint64 tlbelo = LA_PTE_PA(pte);
	if (pte & LA_PTE_V)
		tlbelo |= LA_TLB_V;
	if (pte & LA_PTE_D)
		tlbelo |= LA_TLB_D;

	/* These fields share positions in PTE and TLBELO; use TLB names here. */
	if (pte & LA_PTE_PLV_MASK)
		tlbelo |= pte & LA_TLB_PLV_MASK;
	if (pte & LA_PTE_MAT_MASK)
		tlbelo |= pte & LA_TLB_MAT_MASK;
	if (pte & LA_PTE_G)
		tlbelo |= LA_TLB_G;
	if (pte & LA_PTE_NR)
		tlbelo |= LA_TLB_NR;
	if (pte & LA_PTE_NX)
		tlbelo |= LA_TLB_NX;
	if (pte & LA_PTE_RPLV)
		tlbelo |= LA_TLB_RPLV;

	return tlbelo;
}

#define DMW_VSEG_MASK 0xf000000000000000ULL
#define IS_DMW0_ADDR(va) (((uint64) (va) & DMW_VSEG_MASK) == DMW0_BASE)
#define IS_DMW1_ADDR(va) (((uint64) (va) & DMW_VSEG_MASK) == DMW1_BASE)

#define IS_RAM_PA(pa)                                                          \
	((uint64) (pa) >= DRAM_BASE_LOW && (uint64) (pa) <= PHYSTOP_LOW)

#define IS_DMW0_RAM_VA(va)                                                     \
	(IS_DMW0_ADDR(va) && (uint64) (va) >= (DMW0_BASE | DRAM_BASE_LOW) &&   \
	 (uint64) (va) <= DMW0_PHYSTOP_HIGH)

#define IS_RAM_KVA(va)                                                         \
	((uint64) (va) >= KERNEL_PA2VA(DRAM_BASE_LOW) &&                       \
	 (uint64) (va) <= PHYSTOP_HIGH)

#define DMW0_PA2VA(pa) ((uint64) (pa) | DMW0_BASE)

#define DMW0_VA2PA(va) ((uint64) (va) & LOONGARCH_PA_MASK)

#define KERNEL_PA2VA(pa) ((uint64) (pa) + KERNEL_VIRT_OFFSET)

#define KERNEL_VA2PA(va) ((uint64) (va) - KERNEL_VIRT_OFFSET)

/* The kernel and allocated pages use the high-half direct mapping; DMW0 is
 * reserved for the boot path. */
#define ARCH_PA2KVA(pa) KERNEL_PA2VA(pa)
#define ARCH_KVA2PA(va) KERNEL_VA2PA(va)

extern char _kernel_end_pa[];
#define KERNEL_END ((uint64) KERNEL_PA2VA((uint64) _kernel_end_pa))

void kalloc_init();
uint64 setup_paging();
void paging_init();
void loongarch_vm_init();
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
pte_t *walk_current(uint64 va, int alloc);
int mappages(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size,
	     uint64 perm);
int kvmmap_mmio_current(uint64 va, uint64 pa, uint64 size, uint64 perm);

uint64 walk_addr(pagetable_t pagetable, uint64 va);

#endif
