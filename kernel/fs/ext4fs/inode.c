#include "ext4.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"

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
		LOG_ERROR("ext4_namei: kalloc error");
		return 0;
	}

	info->disk_inode = inode;

	vip->ino = ino;
	vip->count = 1;
	vip->nlinks = 1; // guard
	vip->type = type;
	vip->size = inode.size;
	vip->private_data = info;
	initsleeplock(&vip->lock, "ext4 inode");

	return vip;
}
