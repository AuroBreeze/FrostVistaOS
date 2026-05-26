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

#define EXEC_MAX_PHDRS 16

#define AT_NULL 0
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_ENTRY 9
#define AT_UID 11
#define AT_EUID 12
#define AT_GID 13
#define AT_EGID 14
#define AT_SECURE 23
#define AT_RANDOM 25
#define AT_EXECFN 31

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

		if (readi(inode, 0, PA2VA(pa) + offset, off + i, n) != n) {
			LOG_WARN("loadseg: readi failed");
			return -1;
		}
		i += n;
	}

	return 0;
}

int exec(char *path)
{
	// PERF: Temporary use
#ifdef ROOTFS_EXT4
	if (strcmp(path, "/init") == 0) {
		path = "/musl/basic/getpid";
	}
#endif
	uint64 va_start;
	uint64 va_end = 0;
	struct Process *current_proc = get_proc();

	struct elfhdr eh;
	struct vfs_inode *node = namei(path);
	if (node == 0) {
		LOG_WARN("exec: namei failed");
		return -1;
	}

	pagetable_t user_pagetable = 0;
	struct Process new_layout = {0};

	ilock(node);
	if (readi(node, 0, (uint64) &eh, 0, sizeof(struct elfhdr)) !=
	    sizeof(struct elfhdr)) {
		LOG_WARN("exec: readi failed");
		goto bad_unlock_node;
	}

	if (eh.magic != ELF_MAGIC) {
		LOG_WARN("exec: magic number is not ELF_MAGIC");
		goto bad_unlock_node;
	}
	user_pagetable = uvmcreate();
	if (user_pagetable == 0) {
		LOG_WARN("exec: uvmcreate failed");
		goto bad_unlock_node;
	}

	if (eh.phnum > EXEC_MAX_PHDRS) {
		LOG_WARN("exec: too many program headers: %d", eh.phnum);
		goto bad_unlock_node;
	}

	struct proghdr phs[EXEC_MAX_PHDRS];
	uint64 load_bias = 0;
	int have_load_bias = 0;
	int i;
	int off;
	for (i = 0, off = eh.phoff; i < eh.phnum;
	     i++, off += sizeof(struct proghdr)) {

		if (readi(node, 0, (uint64) &phs[i], off,
			  sizeof(struct proghdr)) != sizeof(struct proghdr)) {
			LOG_WARN("exec: readi proghdr failed");
			goto bad_unlock_node;
		}
	}

	for (i = 0; i < eh.phnum; i++) {
		struct proghdr ph = phs[i];
		if (ph.type != ELF_PROG_LOAD)
			continue;

		if (!have_load_bias) {
			load_bias = ph.vaddr - ph.off;
			have_load_bias = 1;
		}

		va_start = ph.vaddr;
		va_end = va_start + ph.memsz;

		if (!uvmalloc(user_pagetable, va_start, va_end - va_start,
			      flags2perm(ph.flags))) {
			LOG_WARN("exec: uvmalloc segment failed");
			goto bad_unlock_node;
		}

		if (loadseg(user_pagetable, va_start, node, ph.off, ph.filesz) <
		    0) {
			goto bad_unlock_node;
		}
	}

	iunlockput(node);

	uint64 sz = PGROUNDUP(va_end);

	// Protection Page
	if (!uvmalloc(user_pagetable, sz, PGSIZE, 0)) {
		LOG_WARN("uvmalloc failed");
		goto bad;
	}

	uint64 new_heap_bottom = sz + PGSIZE;
	uint64 new_heap_top = sz + PGSIZE;
	uint64 user_stack_top = PHYSTOP_LOW;
	uint64 user_stack_bottom = PHYSTOP_LOW - PGSIZE;

	new_layout.heap_top = new_heap_top;
	if (!uvmalloc(user_pagetable, user_stack_bottom, PGSIZE,
		      PTE_R | PTE_W)) {
		goto bad;
	}

	new_layout.stack_bottom = user_stack_bottom;
	new_layout.stack_top = user_stack_top;

	int argc = 1;
	uint64 sp = user_stack_top;
	int path_len = strlen(path) + 1;

	sp -= path_len;
	if (!copyout(user_pagetable, (char *) sp, (uint64) path, path_len)) {
		goto bad;
	}
	uint64 argv0 = sp;

	uint8 random_bytes[16] = {0};
	sp -= sizeof(random_bytes);
	if (!copyout(user_pagetable, (char *) sp, (uint64) random_bytes,
		     sizeof(random_bytes))) {
		goto bad;
	}
	uint64 random_user = sp;

	sp &= ~0x0F;

	// Linux-style initial stack for ELF _start:
	// argc, argv[], NULL, envp NULL, then auxv key/value pairs.
	uint64 ustack[] = {
	    argc,
	    argv0,
	    0,
	    0,
	    AT_PHDR,
	    load_bias + eh.phoff,
	    AT_PHENT,
	    sizeof(struct proghdr),
	    AT_PHNUM,
	    eh.phnum,
	    AT_PAGESZ,
	    PGSIZE,
	    AT_ENTRY,
	    eh.entry,
	    AT_UID,
	    0,
	    AT_EUID,
	    0,
	    AT_GID,
	    0,
	    AT_EGID,
	    0,
	    AT_SECURE,
	    0,
	    AT_RANDOM,
	    random_user,
	    AT_EXECFN,
	    argv0,
	    AT_NULL,
	    0,
	};

	sp -= sizeof(ustack);
	sp &= ~0x0F;

	if (!copyout(user_pagetable, (char *) sp, (uint64) ustack,
		     sizeof(ustack))) {
		goto bad;
	}

	pagetable_t old_pagetable = current_proc->pagetable;
	struct Process old_layout = *current_proc;

	current_proc->pagetable = user_pagetable;
	current_proc->heap_bottom = new_heap_bottom;
	current_proc->heap_top = new_heap_top;
	current_proc->stack_bottom = user_stack_bottom;
	current_proc->stack_top = user_stack_top;

	current_proc->trapframe->sp = sp;
	current_proc->trapframe->a0 = argc;
	current_proc->trapframe->a1 = sp + sizeof(uint64);
	current_proc->trapframe->epc = eh.entry;

	if (old_layout.heap_top > 0 ||
	    old_layout.stack_top > old_layout.stack_bottom) {
		uvmfree(old_pagetable, &old_layout);
	}

	LOG_TRACE("exec: program loaded to 0x%x", eh.entry);
	return 0;

bad_unlock_node:
	iunlockput(node);
bad:
	if (user_pagetable != 0) {
		if (new_layout.heap_top == 0)
			new_layout.heap_top = PGROUNDUP(va_end);
		uvmfree(user_pagetable, &new_layout);
	}
	return -1;
}
