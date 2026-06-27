#include "kernel/defs.h"
#include "kernel/fs.h"
#include "tmpfs.h"

struct vfs_inode *tmpfs_vfs_lookup(struct vfs_inode *ip, char *name,
				   uint32 *offset)
{
	(void) offset;

	if (ip->type != VFS_DIR)
		return 0;

	struct tmpfs_dirent *de = (struct tmpfs_dirent *) ip->private_data;
	if (de == 0 || de->inode->type != VFS_DIR)
		return 0;

	struct tmpfs_dirent *child = de->inode->dir.children;
	if (child == 0)
		return 0;

	struct list_head *head = child->list.next;
	while (head != &child->list) {
		struct tmpfs_dirent *entry =
		    container_of(head, struct tmpfs_dirent, list);

		if (!namecmp(entry->name, name)) {
			// placeholder
			struct vfs_inode *inode = tmpfs_fill_vfs_inode(
			    entry->ino, entry->inode, entry);
			return inode;
		}

		head = head->next;
	}

	return 0;
}
