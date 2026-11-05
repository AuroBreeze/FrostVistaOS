#ifndef FV_KERNEL_ARCH_VM_H
#define FV_KERNEL_ARCH_VM_H

#include "kernel/types.h"
#include "kernel/arch/mm.h"
#include "asm/vm.h"

/*
 * Architecture-independent page mapping permissions.
 * These values are translated to hardware PTE bits by each architecture.
 */
#define PTE_READ (1ULL << 0)
#define PTE_WRITE (1ULL << 1)
#define PTE_EXEC (1ULL << 2)
#define PTE_USER (1ULL << 3)
#define PTE_VALID (1ULL << 4)
#define PTE_COW (1ULL << 5)

// Encode only the physical page address; permissions are supplied separately.
static inline pte_t arch_pa_to_pte(uint64 pa)
{
	return ARCH_PA2PTE(pa);
}

/* Extract the physical page address, discarding permissions and status bits. */
static inline uint64 arch_pte_to_pa(pte_t pte)
{
	return ARCH_PTE2PA(pte);
}

/*
 * Common kernel interface for converting abstract permissions to PTE flags.
 * The implementation remains in the architecture-specific VM backend.
 */
/* Encode abstract permissions to PTE flags.*/
static inline uint64 arch_pte_from_perm(uint64 perm)
{
	return pte_from_perm(perm);
}

/* Decode leaf PTE permissions; check PTE_VALID before using the mapping. */
static inline uint64 arch_perm_from_pte(pte_t pte)
{
	return perm_from_pte(pte);
}

/*
 * Set or clear one abstract PTE_* permission, preserving unrelated PTE bits.
 * Invalid or combined permission masks leave the PTE unchanged.
 * Returns a new PTE value; callers store it and perform any required TLB flush.
 */
static inline pte_t arch_change_perm_bit(pte_t pte, uint64 bit, int enabled)
{
	return change_perm_bit(pte, bit, enabled);
}

/* Page-table activation is implemented by each architecture backend. */
void switch_to_process(pagetable_t pagetable);
void switch_to_kernel(void);

static inline void arch_switch_to_process(pagetable_t pagetable)
{
	switch_to_process(pagetable);
}

static inline void arch_switch_to_kernel(void)
{
	switch_to_kernel();
}

static inline void arch_kernel_munmapp(pagetable_t pagetable, uint64 va,
				       uint64 size, int do_free_pa)
{
	kvmunmap(pagetable, va, size, do_free_pa);
}

static inline pte_t *arch_page_query(pagetable_t pagetable, uint64 va,
				     int alloc)
{
	return walk(pagetable, va, alloc);
}

static inline int arch_page_map(pagetable_t pagetable, uint64 va, uint64 pa,
				int size, int perm)
{
	return mappages(pagetable, va, pa, size, pte_from_perm(perm));
}

/* Shared lazy page allocation, implemented in kernel/mm/vm.c. */
int handle_page_fault(pagetable_t pagetable, uint64 va);
/* Return 0 for a valid COW mapping, or -1 otherwise. */
int is_cow_fault(pagetable_t pagetable, uint64 va);
int handle_cow_fault(pagetable_t pagetable, uint64 va);

#endif
