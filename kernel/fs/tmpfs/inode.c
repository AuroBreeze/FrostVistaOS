#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/mm/kmalloc.h"
#include "tmpfs.h"

// Explicit inode number allocator. Root occupies TMPFS_ROOT_INO (1); files are
// numbered from 2 upward. Never derived from memory addresses, so two inodes
// can never collide in the icache (which matches on (dev, ino)).
static uint32 tmpfs_next_ino = TMPFS_ROOT_INO + 1;

// clang-format off
struct vfs_inode_ops tmpfs_inode_ops = {
    .lookup = tmpfs_vfs_lookup,
    .stat = 0,
    .create = tmpfs_vfs_create,
    .truncate = 0,
    .unlink = 0,
    .mkdir = 0
};
// clang-format on

struct vfs_file_ops tmpfs_file_ops = {
    .read = 0,
    .write = 0,
    .readdir = 0,
    .lseek = 0,
    .close = 0,
};

struct vfs_superblock_ops tmpfs_superblock_ops = {
    .alloc_inode = 0,
    .destroy_inode = destroy_inode,
    .write_super = 0,
};

struct vfs_inode_ops *get_vfs_inode_ops()
{
	return &tmpfs_inode_ops;
}

struct vfs_file_ops *get_vfs_file_ops()
{
	return &tmpfs_file_ops;
}

struct vfs_superblock_ops *get_vfs_superblock_ops()
{
	return &tmpfs_superblock_ops;
}

void destroy_inode(struct vfs_inode *inode)
{
	struct vfs_inode *root = tmpfs_root();
	if (inode != root)
		// only root cann't be deleted
		put_inode(inode, 0);
}

int tmpfs_vfs_create(struct vfs_inode *dir, char *name, int type)
{
	if (dir == 0 || name == 0 || name[0] == '\0')
		return -1;

	// PERF: The "check-then-act" race condition in the `create` operation:
	// there is a window between the lookup (which acquires and releases an
	// internal directory lock) and the insertion performed while holding
	// the lock; concurrent attempts to create an entry with the same name
	// can result in duplicate directory entries (this is not an issue on
	// single-core systems but occurs in SMP environments).
	struct vfs_inode *existing = tmpfs_vfs_lookup(dir, name, 0);
	vfs_ilock(dir);
	if (existing != 0) {
		put_inode(existing, 0);
		vfs_iunlock(dir);
		return -1;
	}

	// create dir_entry and it's tmpfs_inode
	struct tmpfs_dir_entry *tmpfs_dirent =
	    kmalloc(sizeof(struct tmpfs_dir_entry));

	if (tmpfs_dirent == 0) {
		vfs_iunlock(dir);
		return -1;
	}

	tmpfs_dirent->type = type;

	int n = (strlen(name) >= DIRSIZ) ? DIRSIZ - 1 : strlen(name);
	memmove(&tmpfs_dirent->name, name, n);
	tmpfs_dirent->name[n] = '\0';

	tmpfs_dirent->inode = kmalloc(sizeof(struct tmpfs_inode));
	if (tmpfs_dirent->inode == 0) {
		kmfree(tmpfs_dirent);
		vfs_iunlock(dir);
		return -1;
	}

	tmpfs_dirent->inode->nlinks = 1;
	tmpfs_dirent->inode->type = type;
	tmpfs_dirent->inode->ino = tmpfs_next_ino++;
	tmpfs_dirent->inode->size = 0;
	tmpfs_dirent->inode->dir = tmpfs_dirent;
	memset(tmpfs_dirent->inode->blocks, 0,
	       sizeof(tmpfs_dirent->inode->blocks));
	;

	// mount the tmpfs_inode to directory
	struct tmpfs_dir_entry *dir_entry =
	    ((struct tmpfs_inode *) dir->private_data)->dir;

	struct tmpfs_dir_entry *next_child = dir_entry->child;
	dir_entry->child = tmpfs_dirent;
	tmpfs_dirent->next = next_child;

	vfs_iunlock(dir);

	return 0;
}

/**
 * tmpfs_vfs_lookup - lookup a file in a directory
 *
 * Lock Contract:
 *  Entry: must not hold vfs_inode->lock
 *  Exit: None
 *
 * @dir: the directory to lookup
 * @name: the name to lookup
 *
 * Returns: the inode of the file if found, else 0
 * */
struct vfs_inode *tmpfs_vfs_lookup(struct vfs_inode *dir, char *name,

				   uint32 *offset)
{
	(void) offset;

	if (dir == 0 || dir->type != VFS_DIR)
		return 0;

	if (dir->private_data == 0)
		return 0;

	vfs_ilock(dir);
	struct tmpfs_dir_entry *entry =
	    ((struct tmpfs_inode *) dir->private_data)->dir;

	struct tmpfs_dir_entry *tmp_dir = 0;
	int found = 0;
	for (tmp_dir = entry->child; tmp_dir != 0; tmp_dir = tmp_dir->next) {
		if (namecmp(name, tmp_dir->name) == 0) {
			found = 1;
			break;
		}
	}

	if (!found) {
		vfs_iunlock(dir);
		return 0;
	}

	int ino = tmp_dir->inode->ino;
	struct vfs_inode *tmpfs_inode =
	    tmpfs_fill_vfs_inode(ino, tmp_dir->inode, tmp_dir->inode->type);
	if (tmpfs_inode == 0) {
		vfs_iunlock(dir);
		return 0;
	}
	vfs_iunlock(dir);

	return tmpfs_inode;
}

/**
 * tmpfs_fill_vfs_inode - fill the vfs_inode with the tmpfs_inode
 *
 * Context: During this period, `get_inode` will be used to retrieve the cached
 * inode; therefore, `put_inode` should be used to release it when the inode is
 * no longer in use.
 *
 * @inode: vfs_inode->private_data point to the tmpfs_inode and some date need
 * to fill the vfs_inode
 *
 * Returns: the filled vfs_inode
 * */
struct vfs_inode *tmpfs_fill_vfs_inode(uint32 ino, struct tmpfs_inode *inode,
				       uint8 file_type)
{
	if (inode == 0)
		return 0;

	struct vfs_inode *vip = get_inode(TMPFS_DEV, ino, 0);
	if (!vip)
		return 0;

	vip->dev = TMPFS_DEV;
	vip->ino = ino;
	vip->type = file_type;
	vip->size = inode->size;
	vip->nlinks = inode->nlinks;
	vip->ops = &tmpfs_inode_ops;
	vip->default_f_ops = &tmpfs_file_ops;
	vip->private_data = inode;
	vip->sb = tmpfs_get_root_sb();
	if (vip->sb == 0) {
		put_inode(vip, 0);
		return 0;
	}
	vip->sb->ops = &tmpfs_superblock_ops;

	return vip;
}
