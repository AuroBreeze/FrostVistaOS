#ifndef FV_KERNEL_ARCH_VM_H
#define FV_KERNEL_ARCH_VM_H

#include "kernel/vm.h"
#include "asm/mm.h"
#include "asm/vm.h"

/*
 * Common kernel interface for converting abstract permissions to PTE flags.
 * The implementation remains in the architecture-specific mm.h header.
 */
static inline uint64 arch_pte_from_perm(uint64 perm)
{
	return pte_from_perm(perm);
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

#endif
