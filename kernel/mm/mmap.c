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

int find_empty_vma()
{
	struct Process *proc = get_proc();
	for (int i = 0; i < NVMA; i++) {
		if (proc->vm_area[i].used == 0) {
			return i;
		}
	}
	return -1;
}

struct vm_area_struct *find_vma(uint64 addr, uint64 len)
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

	for (int faddr = used_addr; faddr < proc->stack_bottom;
	     faddr += 0x1000) {
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
			int idx = find_empty_vma();
			if (idx < 0) {
				return 0;
			}
			struct vm_area_struct *vma = &proc->vm_area[idx];
			vma->va_start = faddr;
			vma->va_end = faddr + len;
			vma->used = 1;
			return vma;
		}
	}

	return 0;
}

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

// int do_munmap(uint64 addr, uint64 len);

uint64 do_mmap(uint64 addr, uint64 len, int prot, int flags, int fd,
	       uint64 offset)
{
	addr = PGROUNDUP(addr);
	uint64 length = PGROUNDUP(len);

	struct Process *proc = get_proc();
	pte_t pte = prot2pte(prot);

	if (pte == 0) {
		return -1;
	}
	if ((flags & MAP_TYPE) != MAP_PRIVATE)
		return -1;
	if (!(flags & MAP_ANONYMOUS))
		return -1;
	if (fd != -1 || offset != 0 || len == 0)
		return -1;

	struct vm_area_struct *vma = find_vma(addr, len);
	if (vma == 0) {
		return -1;
	}
	addr = vma->va_start;

	uint64 fail_addr = addr;
	uint64 fail_len = 0;

	for (int i = 0; i < length; i += 4096) {
		uint64 *pa = kalloc();
		if (pa == 0) {
			goto fail;
		}
		if (kvmmap(proc->pagetable, addr, VA2PA(pa), PGSIZE, pte) < 0) {
			kfree(pa);
			goto fail;
		}

		fail_len += 0x1000;
		addr += 4096;
	}
	return vma->va_start;

fail:
	if (fail_addr != 0 && fail_len != 0) {
		kvmunmap(proc->pagetable, fail_addr, fail_len, 1);
	}
	if (vma != 0)
		vma->used = 0;
	return -1;
}
