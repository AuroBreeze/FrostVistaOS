#include "asm/defs.h"
#include "asm/mm.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/types.h"

// linux 2.6.4
#define PROT_READ 0x1  /* page can be read */
#define PROT_WRITE 0x2 /* page can be written */
#define PROT_EXEC 0x4  /* page can be executed */
#define PROT_SEM 0x8   /* page may be used for atomic ops */
#define PROT_NONE 0x0  /* page can not be accessed */

// linux 2.6.4
#define MAP_SHARED 0x01	   /* Share changes */
#define MAP_PRIVATE 0x02   /* Changes are private */
#define MAP_TYPE 0x0f	   /* Mask for type of mapping */
#define MAP_FIXED 0x10	   /* Interpret addr exactly */
#define MAP_ANONYMOUS 0x20 /* don't use a file */

// 128MB - 64MB
#define MMAP_START (64UL * 1024 * 1024)

static uint64 used_addr = MMAP_START;

/**
 * find_free_vma_slot - find an unused VMA slot in the current process
 *
 * Context: Called while creating a new mmap VMA.
 *
 * Return: slot index on success, or -1 when the fixed VMA table is full
 * */
int find_free_vma_slot()
{
	struct Process *proc = get_proc();
	for (int i = 0; i < NVMA; i++) {
		if (proc->vm_area[i].used == 0) {
			return i;
		}
	}
	return -1;
}

/**
 * alloc_vma - initialize a VMA slot for a virtual address range
 *
 * @start : Inclusive virtual start address
 * @end : Exclusive virtual end address
 *
 * Context: The range has already been selected and checked for overlap.
 *
 * Return: initialized VMA pointer on success, or 0 if no VMA slot is free
 * */
struct vm_area_struct *alloc_vma(uint64 start, uint64 end)
{
	struct Process *proc = get_proc();
	int slot = find_free_vma_slot();
	if (slot == -1) {
		return 0;
	}
	struct vm_area_struct *vma = &proc->vm_area[slot];
	// clear the vma
	*vma = (struct vm_area_struct){0};
	vma->va_start = start;
	vma->va_end = end;
	vma->used = 1;

	return vma;
}

/**
 * find_free_range - reserve a free mmap range below the user stack
 *
 * @len : Page-rounded mapping length in bytes
 *
 * Context: Used by mmap when the caller does not request a fixed address.
 *
 * Return: initialized VMA for a non-overlapping range, or 0 on failure
 * */
struct vm_area_struct *find_free_range(uint64 len)
{
	struct Process *proc = get_proc();
	for (uint64 faddr = used_addr; faddr < proc->stack_bottom;
	     faddr += PGSIZE) {
		int found = 1;

		for (int j = 0; j < NVMA; j++) {
			struct vm_area_struct *vma = &proc->vm_area[j];
			if (vma == 0 || vma->used == 0)
				continue;
			if (faddr < vma->va_end &&
			    vma->va_start < faddr + len) {
				found = 0;
				break;
			}
		}

		if (found) {
			struct vm_area_struct *vma =
			    alloc_vma(faddr, faddr + len);
			if (vma == 0) {
				return 0;
			}
			return vma;
		}
	}

	return 0;
}

/**
 * find_overlapping_vma - find the VMA that intersects a virtual range
 *
 * @addr : Virtual start address
 * @len : Range length in bytes
 *
 * Context: Used by munmap and VMA page-fault handling.
 *
 * Return: overlapping VMA pointer, or 0 if no VMA covers the range
 * */
struct vm_area_struct *find_overlapping_vma(uint64 addr, uint64 len)
{
	struct Process *proc = get_proc();
	if (addr != 0) {
		for (int i = 0; i < NVMA; i++) {
			struct vm_area_struct *vma = &proc->vm_area[i];
			if (vma == 0 || vma->used == 0)
				continue;
			if (addr < vma->va_end && vma->va_start < addr + len)
				return vma;
		}
		return 0;
	}

	return 0;
}

/**
 * prot2pte - translate mmap PROT_* bits into user PTE permissions
 *
 * @prot : mmap protection flags
 *
 * Context: Called during mmap argument validation.
 *
 * Return: PTE flags on success, or 0 for unsupported PROT_NONE mappings
 * */
pte_t prot2pte(int prot)
{
	if (prot == PROT_NONE) {
		return 0;
	}
	pte_t flags = PTE_U | PTE_V;

	if (prot & PROT_READ)
		flags |= PTE_R;
	if (prot & PROT_WRITE)
		flags |= PTE_W;
	if (prot & PROT_EXEC)
		flags |= PTE_X;

	return flags;
}

/**
 * do_munmap - remove all or an edge of an mmap VMA
 *
 * @addr : Page-aligned virtual start address to unmap
 * @len : Length in bytes, rounded up to pages
 *
 * Context: Supports whole-VMA unmap and head/tail trimming. Middle splits are
 * rejected until the VMA layer can split one VMA into two records.
 *
 * Return: 0 on success, or -1 for invalid or unsupported ranges
 * */
int do_munmap(uint64 addr, uint64 len)
{
	if (addr % PGSIZE != 0) {
		return -1;
	}
	if (addr == 0 || len == 0) {
		return -1;
	}

	len = PGROUNDUP(len);
	uint64 end = addr + len;

	struct vm_area_struct *vma = find_overlapping_vma(addr, len);
	if (vma == 0) {
		return -1;
	}
	if (addr < vma->va_start || end > vma->va_end)
		return -1;

	// middle split
	if (addr > vma->va_start && end < vma->va_end)
		return -1;

	struct Process *proc = get_proc();
	kvmunmap(proc->pagetable, addr, len, 1);

	if (addr == vma->va_start && end == vma->va_end) {
		if (vma->file != 0)
			fileclose(vma->file);
		vma->used = 0;
	} else if (addr == vma->va_start) {
		if (vma->file != 0)
			vma->file_offset += end - vma->va_start;

		vma->va_start = end;
	} else if (end == vma->va_end) {
		vma->va_end = addr;
	}
	return 0;
}

/**
 * do_mmap - create a lazy private mmap VMA for the current process
 *
 * @addr : Requested start address; only 0 is supported for now
 * @len : Mapping length in bytes
 * @prot : PROT_* permissions requested by userspace
 * @flags : MAP_* mapping flags requested by userspace
 * @fd : File descriptor for file-backed mappings, or -1 for anonymous
 * @offset : File offset for file-backed mappings
 *
 * Context: v1.1 supports anonymous private mappings and private read-only
 * file-backed mappings. Fixed, shared, and writable file mappings are rejected.
 * Physical pages are allocated lazily by the VMA fault handler.
 *
 * Return: virtual start address on success, or -1 on validation/allocation
 * failure
 * */
uint64 do_mmap(uint64 addr, uint64 len, int prot, int flags, int fd,
	       uint64 offset)
{
	if (addr != 0)
		return -1;

	addr = PGROUNDUP(addr);
	uint64 length = PGROUNDUP(len);

	struct Process *proc = get_proc();
	pte_t pte = prot2pte(prot);

	if (pte == 0) {
		return -1;
	}

	if ((offset & (PGSIZE - 1)) != 0)
		return -1;
	if (len == 0)
		return -1;

	int is_anonymous = flags & MAP_ANONYMOUS;

	if ((flags & MAP_TYPE) != MAP_PRIVATE)
		return -1;

	if (is_anonymous) {
		if (fd != -1 || offset != 0)
			return -1;
	} else {
		if (fd < 0 || fd >= NOFILE)
			return -1;
		if (prot & PROT_WRITE)
			return -1;
		struct file *file = proc->ofile[fd];
		if (file == 0) {
			return -1;
		}
	}

	struct vm_area_struct *vma = find_free_range(length);
	if (vma == 0) {
		return -1;
	}
	if (!is_anonymous) {
		if (fd >= 0) {
			struct file *file = proc->ofile[fd];
			vma->file = filedup(file);
		}
		vma->file_offset = offset;
	}

	vma->flags = flags;
	vma->vm_page_prot = pte;
	vma->used = 1;

	return vma->va_start;
}
