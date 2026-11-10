
#include "kernel/errno.h"
#define LOG_MODULE "KERNEL VM"

#include "kernel/list.h"
#include "kernel/vma.h"
#include "kernel/mm/kmalloc.h"
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
	/* Replace the page address while retaining architecture-specific
	 * attributes. */
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

/**
 * handle_file_vma_fault - fault a private or shared file-backed mmap page
 *
 * @vma : File-backed VMA covering the faulting virtual address
 * @va : Page-aligned faulting virtual address
 *
 * Context: The file offset is derived from the VMA base plus the page offset,
 * so page faults are independent of fault order. The page is cleared before
 * reading so EOF or short reads leave the remainder zero-filled.
 *
 * Lock Contract:
 *  Entry: no
 *  running: will lock `file->node->lock` and `file->node->mapping->sleeplock`
 *  exit: will unlock `file->node->lock` and `file->node->mapping->sleeplock`
 *
 * Return: 0 on success, or -1 if allocation, read, or mapping fails
 * */
int handle_file_vma_fault(struct vm_area_struct *vma, uint64 va)
{
	if (va & (ARCH_PGSIZE - 1)) {
		LOG_WARN("handle_file_vma_fault: va is not page aligned");
		return -EINVAL;
	}
	struct file *file = vma->file;
	struct Process *proc = get_proc();

	if (file == 0) {
		LOG_WARN("handle_file_vma_fault: file is null");
		return -ENOENT;
	}

	uint64 off = vma->file_offset + (va - vma->va_start);
	uint64 page_off = off / ARCH_PGSIZE;

	struct address_space *map = vma->file->node->mapping;
	if (map == 0) {
		return -EINVAL;
	}
	if (vma->flags & MAP_SHARED) {
		if (page_off >= 16) {
			return -ESPIPE;
		}
		acquiresleep(&map->sleeplock);

		if (map->addr[page_off] != 0) {
			if (arch_page_map(proc->pagetable, va,
					  arch_kva_to_pa(map->addr[page_off]),
					  ARCH_PGSIZE, vma->vm_page_prot) < 0) {
				LOG_WARN("handle_file_vma_fault: "
					 "kvmmap failed");
				goto failed;
			}
			refcnt_inc(map->addr[page_off]);
			arch_tlb_flush_all();

			releasesleep(&map->sleeplock);
			return 0;
		}
	}

	char *buf = kalloc();
	if (buf == 0) {
		goto failed;
	}
	memset(buf, 0, ARCH_PGSIZE);
	int n = vfs_read_at(file->node, off, (uint8 *) buf, ARCH_PGSIZE);
	if (n < 0) {
		goto freebuf;
	}
	if (arch_page_map(proc->pagetable, va, arch_kva_to_pa((uint64) buf),
			  ARCH_PGSIZE, vma->vm_page_prot) < 0) {
		goto freebuf;
	}

	if (vma->flags & MAP_SHARED) {
		map->addr[page_off] = (uint64) buf;
		refcnt_inc(map->addr[page_off]);
		releasesleep(&map->sleeplock);
	}

	arch_tlb_flush_all();
	return 0;

freebuf:
	kfree(buf);
failed:
	if (vma->flags & MAP_SHARED)
		releasesleep(&map->sleeplock);
	return -1;
}

/**
 * handle_anonymous_vma_fault - allocate one anonymous mmap page
 *
 * @vma : VMA covering the faulting virtual address
 * @va : Page-aligned faulting virtual address
 *
 * Context: Called from handle_vma_fault for MAP_ANONYMOUS VMAs.
 *
 * Return: 0 on success, or -1 if allocation or mapping fails
 * */
int handle_anonymous_vma_fault(struct vm_area_struct *vma, uint64 va)
{
	struct Process *proc = get_proc();
	uint64 *kva = kalloc();
	if (kva == 0) {
		return -1;
	}

	if (arch_page_map(proc->pagetable, va, arch_kva_to_pa((uint64) kva),
			  ARCH_PGSIZE, vma->vm_page_prot) < 0) {
		kfree(kva);
		return -1;
	}
	arch_tlb_flush_all();
	return 0;
}

/**
 * handle_vma_fault - materialize a lazy mmap page for a user fault
 *
 * @va : Faulting virtual address from the trap handler
 *
 * Context: Looks up the covering VMA, rounds the address down to a page, and
 * dispatches to the anonymous or file-backed fault path.
 *
 * Return: 0 on success, or -1 if no VMA covers the fault or mapping fails
 * */
int handle_vma_fault(uint64 va)
{
	va = PGDOWNALIGN(va);
	struct vm_area_struct *vma = find_overlapping_vma(va, ARCH_PGSIZE);
	if (vma == 0) {
		LOG_WARN("handle_vma_fault: no VMA for fault va=%p",
			 (void *) va);
		return -1;
	}

	if (vma->file != 0) {
		return handle_file_vma_fault(vma, va);
	}

	return handle_anonymous_vma_fault(vma, va);
}
