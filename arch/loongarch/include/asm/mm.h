#ifndef __LOONGARCH_MM_H
#define __LOONGARCH_MM_H

#include "asm/machine.h"
#include "kernel/types.h"

#define PGSIZE (4096)
#define LOONGARCH_PALEN 48

/* 仅定义 4 KiB 普通页表项，不包含大页目录项标志。 */
#define LA_PTE_P (1ULL << 0) /* 物理页存在 */
#define LA_PTE_W (1ULL << 1) /* 允许写入 */
#define LA_PTE_PLV_SHIFT 2
#define LA_PTE_PLV_MASK (3ULL << LA_PTE_PLV_SHIFT)
#define LA_PTE_MAT_SHIFT 4
#define LA_PTE_MAT_MASK (3ULL << LA_PTE_MAT_SHIFT)
#define LA_PTE_G (1ULL << 6) /* 全局映射 */
#define LA_PTE_PPN_SHIFT 12
#define LA_PTE_PPN_MASK 0x0000fffffffff000ULL
#define LA_PTE_NR (1ULL << 61)	 /* 不可读 */
#define LA_PTE_NX (1ULL << 62)	 /* 不可执行 */
#define LA_PTE_RPLV (1ULL << 63) /* 限制特权级 */

#define LA_PTE_PLV0 (0ULL << LA_PTE_PLV_SHIFT)
#define LA_PTE_PLV3 (3ULL << LA_PTE_PLV_SHIFT)

#define LA_PTE_MAT_CC (0ULL << LA_PTE_MAT_SHIFT)  /* 一致可缓存 */
#define LA_PTE_MAT_SUC (1ULL << LA_PTE_MAT_SHIFT) /* 强序非缓存 */
#define LA_PTE_MAT_WUC (2ULL << LA_PTE_MAT_SHIFT) /* 弱序非缓存 */

#define LA_PTE_PA(pte) ((uint64) (pte) & LA_PTE_PPN_MASK)
#define LA_PA_PTE(pa) ((uint64) (pa) & LA_PTE_PPN_MASK)

#define DMW_VSEG_MASK 0xf000000000000000ULL
#define IS_DMW0_ADDR(va) (((uint64) (va) & DMW_VSEG_MASK) == DMW0_BASE)
#define IS_DMW1_ADDR(va) (((uint64) (va) & DMW_VSEG_MASK) == DMW1_BASE)

#define IS_RAM_PA(pa)                                                          \
	((uint64) (pa) >= DRAM_BASE_LOW && (uint64) (pa) <= PHYSTOP_LOW)

#define IS_RAM_KVA(va)                                                         \
	(IS_DMW0_ADDR(va) && (uint64) (va) >= (DMW0_BASE | DRAM_BASE_LOW) &&   \
	 (uint64) (va) <= PHYSTOP_HIGH)

#define PA2VA(pa) ((uint64) (pa) | DMW0_BASE)
#define VA2PA(va) ((uint64) (va) & 0x0000ffffffffffffULL)

void kalloc_init();
uint64 setup_paging();
void paging_init();
void loongarch_vm_init();
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
int mappages(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size,
	     uint64 perm);

uint64 walk_addr(pagetable_t pagetable, uint64 va);
#endif
