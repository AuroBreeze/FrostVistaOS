#include "tmpfs.h"
#include "kernel/defs.h"

extern struct vfs_file_ops tmpfs_file_ops;
extern struct vfs_inode_ops tmpfs_inode_ops;
extern struct super_block tmpfs_sb;

void tmpfs_destroy_inode(struct vfs_inode *inode)
{
	put_inode(inode, 0);
}

struct vfs_inode *tmpfs_fill_vfs_inode(uint32 ino, struct tmpfs_inode *tip,
				       struct tmpfs_dirent *de)
{
	if (tip == 0 || ino == 0)
		return 0;

	struct vfs_inode *ip = get_inode(TMPFS_DEV, ino, 0);
	if (ip == 0)
		return 0;
	ip->type = tip->type;
	if (tip->type == VFS_DIR)
		ip->private_data = tip->dir.children;
	else
		ip->private_data = tip->files.files;

	strncpy(ip->name, de->name, DIRSIZ);
	ip->name[DIRSIZ] = '\0';

	ip->ino = ino;
	ip->nlinks = 1;
	ip->sb = &tmpfs_sb;
	ip->default_f_ops = &tmpfs_file_ops;
	ip->ops = &tmpfs_inode_ops;

	return ip;
}
