#include "asm/defs.h"
#include "asm/mm.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/elf.h"
// #include "kernel/init_code.h"
#include "kernel/log.h"
#include "kernel/types.h"

int flags2perm(int flags)
{
	int perm = 0;
	if (flags & 0x1)
		perm |= PTE_X;
	if (flags & 0x2)
		perm |= PTE_W;
	if (flags & 0x4)
		perm |= PTE_R;
	return perm;
}

/**
 * loadseg - Load a segment into pagetable
 * */
static int loadseg(pagetable_t pagetable, uint64 va, struct vfs_inode *inode,
		   uint64 off, uint64 size)
{
	uint64 i = 0;
	while (i < size) {
		uint64 va_page = PGROUNDDOWN(va + i);
		uint64 offset = (va + i) - va_page;

		uint64 pa = walk_addr(pagetable, va_page);
		if (pa == 0) {
			LOG_WARN("loadseg: walk failed");
			return -1;
		}

		uint64 n = PGSIZE - offset;
		if (n > size - i) {
			n = size - i;
		}

		if (readi(inode, 0, PA2VA(pa), off + i, n) != n) {
			LOG_WARN("loadseg: readi failed");
			return -1;
		}
		i += n;
	}

	return 1;
}

int exec(char *path)
{
	uint64 va_start;
	uint64 va_end;
	struct Process *current_proc = get_proc();

	// FIXME: Using `kalloc` will cause a memory leak
	struct elfhdr *eh = kalloc();
	struct vfs_inode *node = namei(path);
	if (node == 0) {
		LOG_WARN("exec: namei failed");
		return -1;
	}

	ilock(node);
	if (readi(node, 0, (uint64) eh, 0, sizeof(struct elfhdr)) !=
	    sizeof(struct elfhdr)) {
		LOG_WARN("exec: readi failed");
		return -1;
	}

	if (eh->magic != ELF_MAGIC) {
		LOG_WARN("exec: magic number is not ELF_MAGIC");
		return -1;
	}
	pagetable_t user_pagetable = uvmcreate();

	int i;
	int off;
	for (i = 0, off = eh->phoff; i < eh->phnum;
	     i++, off += sizeof(struct proghdr)) {

		// FIXME: Using `kalloc` will cause a memory leak
		struct proghdr *ph =
		    kalloc(); // = (struct proghdr *) (init_elf + off);
		if (readi(node, 0, (uint64) ph, off, sizeof(struct proghdr)) !=
		    sizeof(struct proghdr)) {
			LOG_WARN("exec: readi proghdr failed");
			return -1;
		}
		if (ph->type != ELF_PROG_LOAD)
			continue;

		va_start = ph->vaddr;
		va_end = va_start + ph->memsz;

		if (!uvmalloc(user_pagetable, va_start, va_end - va_start,
			      flags2perm(ph->flags))) {
			// Clean up
			uvmdealloc(user_pagetable, va_start, va_end - va_start);
			return -1;
		}

		loadseg(user_pagetable, va_start, node, ph->off, ph->filesz);
	}
	iunlock(node);

	uint64 sz = PGROUNDUP(va_end);

	// Protection Page
	if (!uvmalloc(user_pagetable, sz, PGSIZE, 0)) {
		uvmfree(user_pagetable, current_proc);
		LOG_WARN("uvmalloc failed");
		return -1;
	}

	uint64 new_heap_bottom = sz + PGSIZE;
	uint64 new_heap_top = sz + PGSIZE;
	uint64 user_stack_top = PHYSTOP_LOW;
	uint64 user_stack_bottom = PHYSTOP_LOW - PGSIZE;

	if (!uvmalloc(user_pagetable, user_stack_bottom, PGSIZE,
		      PTE_R | PTE_W)) {
		uvmdealloc(user_pagetable, 0, new_heap_top);
		return -1;
	}

	struct Process new_layout;
	new_layout.heap_top = new_heap_top;
	new_layout.stack_bottom = user_stack_bottom;
	new_layout.stack_top = user_stack_top;

	// Simulated shell
	char *args[] = {"init", "hello", "word", 0};
	int argc = 3;
	uint64 ustack[3 + 1];

	uint64 sp = user_stack_top;

	for (int i = 0; i < argc; i++) {
		int len = strlen(args[i]) + 1;
		sp -= len;
		if (!copyout(user_pagetable, (char *) sp, (uint64) args[i],
			     len)) {
			uvmfree(user_pagetable, &new_layout);
			return 0;
		}
		ustack[i] = sp;
	}

	ustack[argc] = 0;
	sp = sp & ~0x0F;

	int array_size = (argc + 1) * sizeof(uint64);
	sp -= array_size;

	if (!copyout(user_pagetable, (char *) sp, (uint64) ustack,
		     array_size)) {
		uvmfree(user_pagetable, &new_layout);
		return -1;
	}

	pagetable_t old_pagetable = current_proc->pagetable;
	struct Process *old_process = (struct Process *) kalloc();
	*old_process = *current_proc;

	current_proc->pagetable = user_pagetable;
	current_proc->heap_bottom = new_heap_bottom;
	current_proc->heap_top = new_heap_top;
	current_proc->stack_bottom = user_stack_bottom;
	current_proc->stack_top = user_stack_top;

	current_proc->trapframe->sp = sp;
	current_proc->trapframe->a0 = argc;
	current_proc->trapframe->a1 = sp;
	current_proc->trapframe->epc = eh->entry;

	if (old_process->heap_top > 0) {
		uvmfree(old_pagetable, old_process);
		kfree(old_process);
		return -1;
	}

	LOG_TRACE("exec: program loaded to 0x%x", eh->entry);
	return 0;
}
