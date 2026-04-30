#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/fcntl.h"
#define NFILE 128

extern vfs_inode_t *vfs_root;
extern struct spinlock ftable_lock;
extern file_t ftable[NFILE];

int open(const char *path, int flags)
{
	vfs_inode_t *node = vfs_lookup(vfs_root, (char *) path);
	if (node == 0)
		return -1;

	acquire(&ftable_lock);
	int file_id = file_alloc();
	if (file_id == -1)
		return -1;

	file_t *f = &ftable[file_id];

	f->node = node;
	f->offset = 0;
	f->ref_count = 1;
	release(&ftable_lock);

	int mode = flags & O_ACCMODE;

	if (mode == O_RDONLY) {
		f->readable = 1;
		f->writable = 0;
	} else if (mode == O_WRONLY) {
		f->readable = 0;
		f->writable = 1;
	} else if (mode == O_RDWR) {
		f->readable = 1;
		f->writable = 1;
	}

	return alloc_fd(get_proc(), f);
}

int dup(int fd)
{
	if (fd < 0 || fd >= NFILE) {
		return -1;
	}

	struct Process *proc = get_proc();
	if (proc->ofile[fd] == 0) {
		return -1;
	}

	acquire(&ftable_lock);

	int newfd = alloc_fd(proc, proc->ofile[fd]);
	if (newfd == -1) {
		release(&ftable_lock);
		return -1;
	}
	proc->ofile[newfd]->ref_count++;
	release(&ftable_lock);

	return newfd;
}

int filestat(int fd, uint64 user_st_addr)
{
	struct Process *p = get_proc();

	if (fd < 0 || fd >= 16 || p->ofile[fd] == 0)
		return -1;

	file_t *f = p->ofile[fd];
	struct stat st;

	if (f->node->ops->stat) {
		if (f->node->ops->stat(f->node, &st) < 0)
			return -1;
	} else {
		return -1;
	}

	if (copyout(p->pagetable, (char *) user_st_addr, (uint64) &st,
		    sizeof(st)) < 0)
		return -1;

	return 0;
}
