#include "asm/defs.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "tmpfs.h"
#include "asm/mm.h"
#include "core/proc.h"

struct tmpfs_dirent *tmpfs_find_dirent(int ino)
{
	struct tmpfs_dirent *rt = tmpfs_get_root_dirent();
	if (rt == 0)
		return 0;
	int fino = rt->ino;

	while (fino != ino) {
		return 0;
	}

	return 0;
}

int tmp_vfs_read(struct vfs_inode *ip, int user_dst, uint64 dst, uint32 off,
		 uint32 size)
{
	if (ip == 0)
		return -1;
	if (off > ip->size || off + size < off) {
		return -1;
	}
	if (size == 0)
		return 0;

	// If the amount of data read exceeds the space allocated to the current
	// inode
	if (off + size > ip->size) {
		size = ip->size - off;
	}

	struct tmpfs_dirent *tmp = tmpfs_find_dirent(ip->ino);
	if (tmp == 0)
		return -1;
	struct tmpfs_inode *tip = tmp->inode;
	if (tip == 0 || tip->type != VFS_FILE)
		return -1;
	if (tip->files.count == 0)
		return 0;
	struct tmpfs_file *first = tip->files.files;
	if (first == 0)
		return -1;
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
