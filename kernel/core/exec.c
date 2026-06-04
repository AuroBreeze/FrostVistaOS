#include "asm/defs.h"
#include "asm/mm.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/elf.h"
#include "kernel/log.h"
#include "kernel/types.h"

// build_test generates build/gen/kernel/init_code.h.
// Present: exec("/init") loads the embedded user image.
// Absent: exec("/init") is resolved through the mounted filesystem.
#if __has_include("kernel/init_code.h")
#include "kernel/init_code.h"
#define HAVE_EMBEDDED_INIT 1
#else
#define HAVE_EMBEDDED_INIT 0
#endif

#define LOG_MODULE "EXEC"

#define MAX_EXEC_ARGS 16
#define EXEC_STACK_PAGES 2

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

struct elf_reader {
	struct vfs_inode *node;
	const uint8 *data;
	uint64 size;
};

static void free_user_layout(pagetable_t pagetable, uint64 heap_top,
			     uint64 stack_bottom, uint64 stack_top)
{
	if (heap_top > 0) {
		uint64 npage = PGROUNDUP(heap_top) / PGSIZE;
		uvmunmap(pagetable, 0, npage, 1);
	}

	if (stack_top > stack_bottom) {
		uint64 npage = PGROUNDUP(stack_top - stack_bottom) / PGSIZE;
		uvmunmap(pagetable, stack_bottom, npage, 1);
	}

	for (int i = 256; i < 512; i++) {
		pagetable[i] = 0;
	}

	freewalk(pagetable);
}

static int read_elf(struct elf_reader *reader, uint64 dst, uint64 off,
		    uint64 size)
{
	if (reader->data != 0) {
		if (off > reader->size || size > reader->size - off)
			return -1;
		memmove((void *) dst, reader->data + off, size);
		return size;
	}

	return vfs_read_at(reader->node, off, (uint8 *) dst, size);
}

/**
 * loadseg - Load a segment into pagetable
 * */
static int loadseg(pagetable_t pagetable, uint64 va, struct elf_reader *reader,
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

		if (read_elf(reader, PA2VA(pa) + offset, off + i, n) != n) {
			LOG_WARN("loadseg: readi failed");
			return -1;
		}
		i += n;
	}

	return 0;
}

int exec(char *path)
{
	char argv[1][PATH_MAX];

	strncpy(argv[0], path, PATH_MAX);
	argv[0][PATH_MAX - 1] = '\0';

	return execve_kernel(path, argv, 1);
}

int execve_kernel(char *path, char argv[][PATH_MAX], int argc)
{
	if (path == 0 || path[0] == '\0')
		return -1;

	if (argc <= 0 || argc > MAX_EXEC_ARGS)
		return -1;

	if (argv[0][0] == '\0')
		return -1;

	uint64 va_start;
	uint64 va_end = 0;
	struct Process *current_proc = get_proc();
	struct elf_reader reader = {0};

	struct elfhdr eh;
	struct vfs_inode *node = 0;

	pagetable_t user_pagetable = 0;
	uint64 new_heap_top = 0;
	uint64 new_stack_bottom = 0;
	uint64 new_stack_top = 0;

#if HAVE_EMBEDDED_INIT
	if (strcmp(path, "/init") == 0) {
		reader.data = init_code;
		reader.size = init_code_len;
	} else
#endif
	{
		node = vfs_namei(path);
		if (node == 0) {
			LOG_WARN("exec: namei failed");
			return -1;
		}
		reader.node = node;
	}

	if (read_elf(&reader, (uint64) &eh, 0, sizeof(struct elfhdr)) !=
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

		if (read_elf(&reader, (uint64) &phs[i], off,
			     sizeof(struct proghdr)) !=
		    sizeof(struct proghdr)) {
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

		if (uvmalloc(user_pagetable, va_start, va_end - va_start,
			     flags2perm(ph.flags)) < 0) {
			LOG_WARN("exec: uvmalloc segment failed");
			goto bad_unlock_node;
		}

		if (loadseg(user_pagetable, va_start, &reader, ph.off,
			    ph.filesz) < 0) {
			goto bad_unlock_node;
		}
	}

	if (node != 0) {
		// iunlockput(node);
		vfs_iput(node);
		node = 0;
	}

	uint64 sz = PGROUNDUP(va_end);

	// Protection Page
	if (uvmalloc(user_pagetable, sz, PGSIZE, 0) < 0) {
		LOG_WARN("uvmalloc failed");
		goto bad;
	}

	uint64 new_heap_bottom = sz + PGSIZE;
	new_heap_top = sz + PGSIZE;
	uint64 user_stack_top = PHYSTOP_LOW;
	uint64 user_stack_bottom = PHYSTOP_LOW - EXEC_STACK_PAGES * PGSIZE;

	if (uvmalloc(user_pagetable, user_stack_bottom,
		     EXEC_STACK_PAGES * PGSIZE, PTE_R | PTE_W) < 0) {
		goto bad;
	}

	new_stack_bottom = user_stack_bottom;
	new_stack_top = user_stack_top;

	uint64 sp = user_stack_top;
	uint64 argv_ptr[MAX_EXEC_ARGS + 1] = {0};
	for (int i = argc - 1; i >= 0; i--) {
		int len = strlen(argv[i]) + 1;
		sp -= len;
		sp &= ~0x0FUL;
		if (sp < user_stack_bottom)
			goto bad;

		if (copyout(user_pagetable, (char *) sp, (uint64) argv[i],
			    len) < 0)
			goto bad;

		argv_ptr[i] = sp;
	}
	argv_ptr[argc] = 0;

	uint8 random_bytes[16] = {0};
	sp -= sizeof(random_bytes);
	if (sp < user_stack_bottom)
		goto bad;
	if (copyout(user_pagetable, (char *) sp, (uint64) random_bytes,
		    sizeof(random_bytes)) < 0) {
		goto bad;
	}
	uint64 random_user = sp;

	sp &= ~0x0F;

	// Linux-style initial stack for ELF _start:
	// argc, argv[], NULL, envp NULL, then auxv key/value pairs.
	uint64 ustack[1 + MAX_EXEC_ARGS + 1 + 1 + 24] = {0};
	int n = 0;
	ustack[n++] = argc;
	for (int i = 0; i < argc; i++)
		ustack[n++] = argv_ptr[i];
	ustack[n++] = 0; // argv terminator
	ustack[n++] = 0; // envp terminator; envp is not populated yet
	ustack[n++] = AT_PHDR;
	ustack[n++] = load_bias + eh.phoff;
	ustack[n++] = AT_PHENT;
	ustack[n++] = sizeof(struct proghdr);
	ustack[n++] = AT_PHNUM;
	ustack[n++] = eh.phnum;
	ustack[n++] = AT_PAGESZ;
	ustack[n++] = PGSIZE;
	ustack[n++] = AT_ENTRY;
	ustack[n++] = eh.entry;
	ustack[n++] = AT_UID;
	ustack[n++] = 0;
	ustack[n++] = AT_EUID;
	ustack[n++] = 0;
	ustack[n++] = AT_GID;
	ustack[n++] = 0;
	ustack[n++] = AT_EGID;
	ustack[n++] = 0;
	ustack[n++] = AT_SECURE;
	ustack[n++] = 0;
	ustack[n++] = AT_RANDOM;
	ustack[n++] = random_user;
	ustack[n++] = AT_EXECFN;
	ustack[n++] = argv_ptr[0];
	ustack[n++] = AT_NULL;
	ustack[n++] = 0;

	sp -= n * sizeof(uint64);
	sp &= ~0x0F;
	if (sp < user_stack_bottom)
		goto bad;

	if (copyout(user_pagetable, (char *) sp, (uint64) ustack,
		    n * sizeof(uint64)) < 0) {
		goto bad;
	}

	pagetable_t old_pagetable = current_proc->pagetable;
	// NOTE: Do not copy struct Process here. The fd table is part of
	// Process, so raising NOFILE can make a full stack copy overflow the
	// one-page kernel stack during fork+exec. Only these layout fields are
	// needed to release the old user page table.
	uint64 old_heap_top = current_proc->heap_top;
	uint64 old_stack_bottom = current_proc->stack_bottom;
	uint64 old_stack_top = current_proc->stack_top;

	current_proc->pagetable = user_pagetable;
	uvmswitch(user_pagetable);
	current_proc->heap_bottom = new_heap_bottom;
	current_proc->heap_top = new_heap_top;
	current_proc->stack_bottom = user_stack_bottom;
	current_proc->stack_top = user_stack_top;

	current_proc->trapframe->sp = sp;
	current_proc->trapframe->a0 = argc;
	current_proc->trapframe->a1 = sp + sizeof(uint64);
	current_proc->trapframe->epc = eh.entry;

	if (old_heap_top > 0 || old_stack_top > old_stack_bottom) {
		free_user_layout(old_pagetable, old_heap_top, old_stack_bottom,
				 old_stack_top);
	}

	LOG_TRACE("exec: program loaded to 0x%x", eh.entry);
	return 0;

bad_unlock_node:
	if (node != 0)
		// iunlockput(node);
		vfs_iput(node);
bad:
	if (user_pagetable != 0) {
		if (new_heap_top == 0)
			new_heap_top = PGROUNDUP(va_end);
		free_user_layout(user_pagetable, new_heap_top, new_stack_bottom,
				 new_stack_top);
	}
	return -1;
}
