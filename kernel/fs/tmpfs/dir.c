#include "kernel/defs.h"
#include "kernel/fs.h"
#include "tmpfs.h"

struct vfs_inode *tmpfs_vfs_lookup(struct vfs_inode *ip, char *name,
				   uint32 *offset)
{
	(void) offset;
	if (ip == 0)
		return 0;
	if (ip->type != VFS_DIR)
		return 0;

	struct tmpfs_dirent *child = (struct tmpfs_dirent *) ip->private_data;
	if (child == 0)
		return 0;

	struct tmpfs_dirent *entry = child;
	while (1) {

		if (!namecmp(entry->name, name)) {
			struct vfs_inode *inode = tmpfs_fill_vfs_inode(
			    entry->ino, entry->inode, entry);
			return inode;
		}
		struct list_head *next = entry->list.next;
		if (next == &child->list)
			break;
		entry = container_of(next, struct tmpfs_dirent, list);
	}

	return 0;
}
