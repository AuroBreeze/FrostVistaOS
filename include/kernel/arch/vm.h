#ifndef __KERNEL_ARCH_VM_H__
#define __KERNEL_ARCH_VM_H__

#include "kernel/vm.h"
#include "asm/defs.h"
#include "asm/mm.h"

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

#endif
