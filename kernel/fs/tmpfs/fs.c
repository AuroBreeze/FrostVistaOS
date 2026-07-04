#include "asm/defs.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "tmpfs.h"
#include "asm/mm.h"
#include "core/proc.h"

struct tmpfs_dirent *tmpfs_find_dirent(uint32 ino, struct tmpfs_dirent *base)
{
	if (base == 0)
		return 0;

	struct tmpfs_dirent *stack[32] = {0};
	int sp = 0;

	stack[sp++] = base;

	while (sp > 0) {
		struct tmpfs_dirent *first = stack[--sp];
		struct tmpfs_dirent *entry = first;

		while (1) {
			if (entry->ino == ino)
				return entry;

			struct tmpfs_inode *tip = entry->inode;
			if (tip != 0 && tip->type == VFS_DIR &&
			    tip->dir.children != 0) {
				if (sp >= 32)
					return 0;
				stack[sp++] = tip->dir.children;
			}

			struct list_head *next = entry->list.next;
			if (next == &first->list)
				break;

			entry = container_of(next, struct tmpfs_dirent, list);
		}
	}

	return 0;
}

int tmpfs_vfs_read(struct file *f, uint8 *buffer, uint32 size)
{
	if (f == 0 || f->node == 0 || buffer == 0) {
		return -1;
	}

	// NOTE: At this stage, the CPU is not yet initialized, so sleeplocks
	// cannot be used.
	int n = tmpfs_read(f->node, 0, (uint64) buffer, f->offset, size);

	return n;
}

int tmpfs_read(struct vfs_inode *ip, int user_dst, uint64 dst, uint32 off,
	       uint32 size)
{
	if (ip == 0)
		return -1;
	if (off > ip->size || off + size < off) {
		return -1;
	}
	if (size == 0)
		return 0;

	// TODO: tmpfs inode population does not initialize inode->size yet.
	// Temporarily skip the following test code until size propagation is
	// designed and implemented.
	//
	// If the amount of data read exceeds the space allocated to the current
	// inode
	// if (off + size > ip->size) {
	// 	size = ip->size - off;
	// }

	struct tmpfs_dirent *rt = tmpfs_get_root_dirent();
	struct tmpfs_dirent *tmp = tmpfs_find_dirent(ip->ino, rt);
	if (tmp == 0) {
		LOG_WARN("tmpfs_read: tmp == 0");
		return -1;
	}
	struct tmpfs_inode *tip = tmp->inode;
	if (tip == 0 || tip->type != VFS_FILE) {
		LOG_WARN("tmpfs_read: tip->type != VFS_FILE or tip == 0");
		return -1;
	}
	if (tip->files.count == 0)
		return 0;
	struct tmpfs_file *first = tip->files.files;
	if (first == 0) {
		LOG_WARN("tmpfs_read: first == 0");
		return -1;
	}
	struct tmpfs_file *file = first;

	uint npages = off / PGSIZE;
	for (int i = 0; i < npages; i++) {
		if (file == 0)
			return -1;
		struct list_head *next = file->list.next;
		if (next == &first->list)
			return -1;
		file = container_of(next, struct tmpfs_file, list);
	}

	uint32 tot;
	uint32 m;
	for (tot = 0; tot < size; tot += m, off += m, dst += m) {
		if (file == 0)
			return tot == size ? tot : -1;

		uint64 addr = (uint64) file->space;
		if (addr == 0)
			return -1;

		m = (size - tot) > (PGSIZE - (off % PGSIZE))
			? PGSIZE - (off % PGSIZE)
			: size - tot;

		if (user_dst) {
			struct Process *proc = get_proc();
			if (copyout(proc->pagetable, (void *) dst,
				    (uint64) (addr + (off % PGSIZE)),
				    (int) m) < 0) {
				return -1;
			}
		} else {
			memmove((void *) dst,
				(const void *) (addr + (off % PGSIZE)), m);
		}

		struct list_head *next = file->list.next;
		if (next == &first->list) {
			// Because the function returns early before the for
			// loop applies + m, the return value needs to add it
			// explicitly.
			if (tot + m < size)
				return -1;
			return tot + m;
		}

		file = container_of(next, struct tmpfs_file, list);
	}

	return tot;
}
