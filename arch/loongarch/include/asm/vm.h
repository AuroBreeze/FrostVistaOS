#ifndef FV_LOONGARCH_VM_H
#define FV_LOONGARCH_VM_H

/*
 * Configure a three-level page table for the lower address space:
 *
 *   VA[38:30] -> Dir2
 *   VA[29:21] -> Dir1
 *   VA[20:12] -> PT
 *   VA[11:0]  -> page offset
 *
 * The active address space uses PGDL and PGDH to manage the lower and upper
 * address spaces, respectively.
 */
#include "kernel/types.h"

struct Process;

#define LA_PAGE_SHIFT 12 // 4 KiB pages
#define LA_PT_WIDTH 9
#define LA_DIR1_BASE 21
#define LA_DIR1_WIDTH 9
#define LA_DIR2_BASE 30
#define LA_DIR2_WIDTH 9
#define LA_PT_ENTRIES (1 << LA_PT_WIDTH)
/*
 * Three 9-bit page-table levels with 4 KiB pages use VA[38:0], so VALEN is
 * 39. VA[38] is the sign bit: the lower half spans [0, 2^38), and the upper
 * half begins at 0xffffffc000000000. All other addresses are noncanonical.
 */
#define LA_VALEN (LA_PAGE_SHIFT + LA_PT_WIDTH + LA_DIR1_WIDTH + LA_DIR2_WIDTH)
#define LA_VA_SIGN_BIT (LA_VALEN - 1)
#define LA_LOW_VA_LIMIT (1ULL << LA_VA_SIGN_BIT)
#define LA_HIGH_VA_BASE (~(LA_LOW_VA_LIMIT - 1ULL))

static inline __attribute__((always_inline)) int loongarch_is_low_va(uint64 va)
{
	return va < LA_LOW_VA_LIMIT;
}

static inline __attribute__((always_inline)) int loongarch_is_high_va(uint64 va)
{
	return va >= LA_HIGH_VA_BASE;
}

#define LA_PWCL_FIELD(value, shift) ((uint64) (value) << (shift))

/* CRMD address translation mode bits. */
#define CRMD_DA (1ULL << 3)
#define CRMD_PG (1ULL << 4)

static inline
    __attribute__((always_inline)) uint64 loongarch_vpn(uint64 va, int level)
{
	return (va >> (LA_PAGE_SHIFT + (level * LA_PT_WIDTH))) &
	       (LA_PT_ENTRIES - 1);
}

int kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size,
	   uint64 perm);
int kvmmap_mmio_current(uint64 va, uint64 pa, uint64 size, uint64 perm);

pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
pte_t *walk_current(uint64 va, int alloc);
int mappages(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size,
	     uint64 perm);
uint64 walk_addr(pagetable_t pagetable, uint64 va);
void kvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free_pa);
void uvmunmap(pagetable_t pagetable, uint64 va, int npage, int do_free);
pagetable_t uvmcreate(void);
int uvmdealloc(pagetable_t pagetable, uint64 va, uint64 size);
int uvmalloc(pagetable_t pagetable, uint64 va, uint64 size, uint64 perm);
void freewalk(pagetable_t pagetable);
void uvmfree(pagetable_t pagetable, struct Process *p);
int uvmcopy(pagetable_t old, pagetable_t new);
int copyout(pagetable_t pagetable, char *dst, uint64 src, int len);
int copyin(pagetable_t pagetable, char *dst, uint64 src, int len);
int handle_page_fault(pagetable_t pagetable, uint64 va);
int handle_vma_fault(uint64 va);
int is_cow_fault(pagetable_t pagetable, uint64 va);
int handle_cow_fault(pagetable_t pagetable, uint64 va);

void device_mapping();
#endif
