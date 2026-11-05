
#define LOG_MODULE "KERNEL VM"

#include "kernel/arch/vm.h"
#include "kernel/arch/mm.h"
#include "kernel/defs.h"
#include "kernel/types.h"
#include "kernel/log.h"
#include "kernel/proc.h"
#include "kernel/string.h"

/**
 * handle_page_fault - Handle page fault
 *
 * @pagetable : Base address of the target pagetable
 * @va : Virtual address
 *
 * Context: Used to handle page fault
 *
 * Return: if success, return 0, otherwise return -1
 * */
int handle_page_fault(pagetable_t pagetable, uint64 va)
{
	va = PGDOWNALIGN(va);

	struct Process *current_proc = get_proc();
	if (va > current_proc->stack_bottom) {
		LOG_WARN("handle_page_fault: va out of range");
		return -1;
	}
	if (va < current_proc->heap_bottom) {
		LOG_WARN("handle_page_fault: va out of range");
		return -1;
	}

	char *mem = kalloc();
	if (mem == 0) {
		return -1;
	}

	if (arch_page_map(pagetable, va, arch_kva_to_pa((uint64) mem),
			  ARCH_PGSIZE, PTE_READ | PTE_WRITE | PTE_USER) < 0) {
		kfree(mem);
		return -1;
	}

	arch_tlb_flush_all();

	return 0;
}

int is_cow_fault(pagetable_t pagetable, uint64 va)
{
	LOG_TRACE("is_cow_fault: va: %p", (void *) va);
	va = PGDOWNALIGN(va);
	pte_t *pte = arch_page_query(pagetable, va, 0);
	if (pte == 0) {
		LOG_DEBUG("is_cow_fault: walk failed");
		return -1;
	}
	uint64 perm = arch_perm_from_pte(*pte);
	if (!(perm & PTE_VALID) || !(perm & PTE_USER) || !(perm & PTE_COW) ||
	    (perm & PTE_WRITE))
		return -1;

	return 0;
}

int handle_cow_fault(pagetable_t pagetable, uint64 va)
{
	LOG_TRACE("handle_cow_fault: va: %p", (void *) va);
	va = PGDOWNALIGN(va);
	pte_t *pte = arch_page_query(pagetable, va, 0);
	if (!pte) {
		LOG_WARN("handle_cow_fault: walk failed");
		return -1;
	}

	uint64 pa = arch_pte_to_pa(*pte);
	uint64 flags = arch_perm_from_pte(*pte);

	if (!(flags & PTE_VALID) || !(flags & PTE_USER) || !(flags & PTE_COW) ||
	    (flags & PTE_WRITE))
		return -1;

	char *mem = kalloc();
	if (mem == 0) {
		return -1;
	}

	memmove(mem, (void *) arch_pa_to_kva(pa), ARCH_PGSIZE);
	/* Replace the page address while retaining architecture-specific attributes. */
	pte_t replacement = (*pte & ~arch_pa_to_pte(pa)) |
			    arch_pa_to_pte(arch_kva_to_pa((uint64) mem));
	replacement = arch_change_perm_bit(replacement, PTE_COW, 0);
	replacement = arch_change_perm_bit(replacement, PTE_WRITE, 1);
	*pte = replacement;

	arch_tlb_flush_all();
	kfree((void *) arch_pa_to_kva(pa));

	LOG_TRACE("handle_cow_fault: success");
	return 0;
}
