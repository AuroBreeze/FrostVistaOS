#include "ext4.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"

static struct vfs_inode *ext4_inode_to_vfs(uint32 ino, struct ext4_inode *inode,
					   uint8 file_type);

static int ext4_vfs_read(struct file *f, uint8 *buffer, uint32 size)
{
	struct ext4_fs *fs = ext4_get_root_fs();
	struct ext4_inode_info *info;

	if (fs == 0 || f == 0 || f->node == 0) {
		return -1;
	}

	info = (struct ext4_inode_info *) f->node->private_data;
	if (info == 0) {
		return -1;
	}

	return ext4_read_file(fs, &info->disk_inode, f->offset, buffer, size);
}

static int ext4_vfs_stat(struct vfs_inode *node, struct stat *st)
{
	if (node == 0 || st == 0) {
		return -1;
	}

	st->addr = 0;
	st->type = node->type == VFS_DIR ? T_DIR : T_FILE;
	st->nlink = node->nlinks;
	st->size = node->size;
	return 0;
}

struct vfs_inode *ext4_vfs_lookup(struct vfs_inode *dir, char *name,
				  uint32 *offset)
{
	struct ext4_fs *fs;
	if ((fs = ext4_get_root_fs()) == 0) {
		LOG_ERROR("ext4_vfs_lookup: ext4_get_root_fs error");
		return 0;
	}

	if (dir == 0 || dir->type != VFS_DIR || dir->private_data == 0) {
		return 0;
	}

	struct ext4_inode_info *dir_info =
	    (struct ext4_inode_info *) dir->private_data;
	struct ext4_inode inode;
	uint8 file_type;
	uint32 ino;

	if (offset != 0) {
		*offset = 0;
	}

	if (ext4_lookup_in_dir(fs, &dir_info->disk_inode, name, &ino,
			       &file_type) < 0) {
		return 0;
	}

	if (ext4_read_inode(fs, ino, &inode) < 0) {
		return 0;
	}

	return ext4_inode_to_vfs(ino, &inode, file_type);
}

static struct vfs_inode_ops ext4_inode_ops = {
    .lookup = ext4_vfs_lookup,
    .stat = ext4_vfs_stat,
};

static struct vfs_file_ops ext4_file_ops = {
    .read = ext4_vfs_read,
    .write = 0,
    .readdir = 0,
    .lseek = 0,
    .close = 0,
};

static struct vfs_inode *ext4_inode_to_vfs(uint32 ino, struct ext4_inode *inode,
					   uint8 file_type)
{
	short type;
	if (file_type == EXT4_FT_DIR) {
		type = VFS_DIR;
	} else if (file_type == EXT4_FT_REG_FILE) {
		type = VFS_FILE;
	} else {
		return 0;
	}

	struct vfs_inode *vip = kalloc();
	if (!vip)
		return 0;

	struct ext4_inode_info *info = kalloc();
	if (!info) {
		kfree(vip);
		LOG_ERROR("ext4_inode_to_vfs: kalloc error");
		return 0;
	}

	info->disk_inode = *inode;

	vip->ino = ino;
	vip->count = 1;
	vip->nlinks = 1; // guard
	vip->type = type;
	vip->size = inode->size;
	vip->ops = &ext4_inode_ops;
	vip->default_f_ops = &ext4_file_ops;
	vip->private_data = info;
	initsleeplock(&vip->lock, "ext4 inode");

	return vip;
}

/* ext4_namei - Look up and return the inode for a path name */
struct vfs_inode *ext4_namei(char *path)
{
	struct ext4_fs *fs;
	if ((fs = ext4_get_root_fs()) == 0) {
		LOG_ERROR("ext4_namei: ext4_get_root_fs error");
		return 0;
	}

	struct ext4_inode inode;
	uint8 file_type;
	uint32 ino;

	if (ext4_lookup_path_ino(fs, path, &inode, &file_type, &ino) < 0) {
		LOG_ERROR("ext4_namei: ext4_lookup_path_ino error");
		return 0;
	}

	return ext4_inode_to_vfs(ino, &inode, file_type);
}
