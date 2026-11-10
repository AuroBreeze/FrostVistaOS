#include "kernel/errno.h"
#include "kernel/arch/mm.h"
#include "kernel/arch/vm.h"
#include "kernel/list.h"
#include "kernel/log.h"
#include "kernel/mm/kmalloc.h"
#include "kernel/proc.h"
#include "kernel/vma.h"
#include "kernel/defs.h"
#include "kernel/types.h"
#include "kernel/string.h"

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
	*vma = (struct vm_area_struct) {0};
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
	if (used_addr > proc->stack_bottom ||
	    len > proc->stack_bottom - used_addr) {
		LOG_WARN("mmap range too large");
		return 0;
	}

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
	pte_t flags = PTE_USER;

	if (prot & PROT_READ)
		flags |= PTE_READ;
	if (prot & PROT_WRITE)
		flags |= PTE_WRITE;
	if (prot & PROT_EXEC)
		flags |= PTE_EXEC;

	return flags;
}

int writeback_vma_shared_mapping(pagetable_t pagetable,
				 struct vm_area_struct *vma, uint64 addr,
				 uint64 len)
{
	len = PGROUNDUP(len);
	uint64 end = addr + len;

	uint64 file_size = vma->file->node->size;
	for (uint64 page_va = addr; page_va < end; page_va += PGSIZE) {
		uint64 pa = walk_addr(pagetable, page_va);
		if (pa == 0)
			continue;

		uint64 offset = vma->file_offset + (page_va - vma->va_start);
		if (offset >= file_size)
			continue;

		uint64 count = end - page_va;
		if (count > PGSIZE)
			count = PGSIZE;
		if (count > file_size - offset)
			count = file_size - offset;

		uint8 *buf = (uint8 *) arch_pa_to_kva(pa);
		int ret = vfs_write_at(vma->file, offset, buf, count);
		if (ret < 0)
			return ret;
		if ((uint64) ret != count)
			return -EIO;
	}

	return 0;
}

/**
 * get_or_create_mapping - get or create a mapping for a VMA
 *
 * Lock Contract:
 *  Entry: no
 *  running: will lock `file->node->lock`
 *  exit: will unlock `file->node->lock`
 *
 * Return: the mapping on success, or 0 on failure
 * */
static struct address_space *get_or_create_mapping(struct vm_area_struct *vma)
{
	struct vfs_inode *node = vma->file->node;
	acquiresleep(&node->lock);
	struct address_space *mapping = vma->file->node->mapping;
	if (!mapping) {
		mapping = kmalloc(sizeof(struct address_space));
		if (mapping == 0) {
			releasesleep(&node->lock);
			LOG_WARN("get_or_create_mapping: kmalloc failed");
			return 0;
		}
		memset(mapping, 0, sizeof(struct address_space));
		initsleeplock(&mapping->sleeplock, "mapping sleeplock");
		list_init(&mapping->mapped_head);
		vma->file->node->mapping = mapping;
	}
	releasesleep(&node->lock);
	return mapping;
}

/**
 * register_mapped_vma - register a VMA as mapped in a mapping
 *
 * Context: When performing a `truncate` operation, it is necessary to modify
 * the file mappings held by multiple processes. Therefore, a `mapped_vma`
 * structure is required to keep track of which processes have mapped specific
 * files, enabling the necessary adjustments to those mappings.
 *
 * Lock Contract:
 *  Entry: no
 *  running: will lock `mapping->sleeplock`
 *  exit: will unlock `mapping->sleeplock`
 *
 * Return: 0 on success, or -1 on failure
 * */
int register_mapped_vma(struct Process *proc, struct vm_area_struct *vma)
{
	struct address_space *mapping = get_or_create_mapping(vma);
	if (!mapping) {
		return -ENOMEM;
	}

	acquiresleep(&mapping->sleeplock);
	struct mapped_vma *mapped = kmalloc(sizeof(struct mapped_vma));
	if (mapped == 0) {
		releasesleep(&mapping->sleeplock);
		return -ENOMEM;
	}
	mapped->proc = proc;
	mapped->vma = vma;
	list_add_tail(&mapped->head, &mapping->mapped_head);
	releasesleep(&mapping->sleeplock);

	return 0;
}

/**
 * remove_mapped_vma - remove a VMA from a mapping
 *
 * Lock Contract:
 *  Entry: None
 *  Running: will lock `mapping->sleeplock`
 *  Exit: will unlock `mapping->sleeplock`
 * */
int remove_mapped_vma(struct Process *proc, struct vm_area_struct *vma)
{
	struct address_space *mapping = vma->file->node->mapping;
  if(mapping == 0){
    return 0;
  }
	acquiresleep(&mapping->sleeplock);
	struct list_head *pos;
	struct list_head *next;
	list_for_each_safe(pos, next, &mapping->mapped_head)
	{
		struct mapped_vma *mapped =
		    container_of(pos, struct mapped_vma, head);
		if (mapped->proc == proc && mapped->vma == vma) {
			list_del(&mapped->head);
			kmfree(mapped);
		}
	}
	releasesleep(&mapping->sleeplock);
	return 0;
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
 * Return: 0 on success, or a negative errno for invalid or unsupported ranges
 * */
int do_munmap(uint64 addr, uint64 len)
{
	if (addr & (PGSIZE - 1)) {
		return -EINVAL;
	}
	if (addr == 0 || len == 0) {
		return -EINVAL;
	}

	len = PGROUNDUP(len);
	uint64 end = addr + len;

	struct vm_area_struct *vma = find_overlapping_vma(addr, len);
	if (vma == 0) {
		return -EINVAL;
	}
	if (addr < vma->va_start || end > vma->va_end)
		return -EINVAL;

	// middle split
	if (addr > vma->va_start && end < vma->va_end)
		return -EOPNOTSUPP;

	struct Process *proc = get_proc();

	if (vma->file != 0 && (vma->flags & MAP_TYPE) == MAP_SHARED &&
	    (vma->vm_page_prot & PTE_WRITE)) {
		int ret;
		if ((ret = writeback_vma_shared_mapping(proc->pagetable, vma,
							addr, len)) < 0)
			return ret;
	}

	// FIXME: The record of a file's mapping is removed only when the entire
	// VMA is released.
	if (addr == vma->va_start && end == vma->va_end) {
		if (vma->file != 0) {
			remove_mapped_vma(proc, vma);
			fileclose(vma->file);
			vma->file = 0;
		}
		vma->used = 0;
	}

	arch_kernel_munmapp(proc->pagetable, addr, len, 1);
	/* A later mmap can reuse this range with different physical
	 * pages. */
	arch_tlb_flush_all();

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
 * do_mmap - create a lazy mmap VMA for the current process
 *
 * @addr : Requested start address; only 0 is supported for now
 * @len : Mapping length in bytes
 * @prot : PROT_* permissions requested by userspace
 * @flags : MAP_* mapping flags requested by userspace
 * @fd : File descriptor for file-backed mappings, or -1 for anonymous
 * @offset : File offset for file-backed mappings
 *
 * Context: Supports private and shared mappings with lazy page allocation.
 * Shared writable file pages are written back on munmap; pages are not yet
 * shared between separate mappings. Nonzero requested addresses are rejected.
 *
 * Return: virtual start address on success, or a negative errno on
 * validation/allocation failure
 * */
uint64 do_mmap(uint64 addr, uint64 len, int prot, int flags, int fd,
	       uint64 offset)
{
	if (addr != 0)
		return -EOPNOTSUPP;

	addr = PGROUNDUP(addr);
	uint64 length = PGROUNDUP(len);

	struct Process *proc = get_proc();
	pte_t pte = prot2pte(prot);

	if (pte == 0) {
		return -EINVAL;
	}

	if ((offset & (PGSIZE - 1)) != 0)
		return -EINVAL;
	if (len == 0)
		return -EINVAL;

	int is_anonymous = flags & MAP_ANONYMOUS;

	if ((flags & MAP_TYPE) != MAP_PRIVATE &&
	    (flags & MAP_TYPE) != MAP_SHARED)
		return -EOPNOTSUPP;

	if (is_anonymous) {
		if (fd != -1 || offset != 0)
			return -EINVAL;
	} else {
		if (fd < 0 || fd >= PROCESS_MAX_OPEN_FILES)
			return -EBADF;
		struct file *file = proc->ofile[fd];
		if (file == 0) {
			return -EBADF;
		}
		if (!file->readable)
			return -EACCES;
		if ((flags & MAP_TYPE) == MAP_SHARED && (prot & PROT_WRITE) &&
		    !file->writable) {
			return -EACCES;
		}
	}

	struct vm_area_struct *vma = find_free_range(length);
	if (vma == 0) {
		return -ENOMEM;
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

	if (!is_anonymous) {
		int ret = register_mapped_vma(get_proc(), vma);
		if (ret < 0) {
			fileclose(vma->file);
			vma->used = 0;
			return ret;
		}
	}
	return vma->va_start;
}
