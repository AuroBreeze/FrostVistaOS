// mount easyfs
#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "easyfs.h"
#include "kernel/fs.h"
#include "kernel/log.h"

static struct disk_super_block easyfs_root_fs;
static int easyfs_root_mounted = 0;
extern struct vfs_inode *vfs_root;
static struct super_block sb = {0};

static int easyfs_mount()
{
	struct buf *b = bread(EASYFS_DEV, SUPER_BLOCK);
	struct disk_super_block *dsb;

	dsb = (struct disk_super_block *) b->data;
	easyfs_root_fs = *dsb;

	if (dsb->magic == EASYFS_MAGIC) {
		easyfs_root_mounted = 1;
		LOG_TRACE("mount_easyfs: Magic number 0x0B8EE2E0 matched!");
		brelse(b);
		return 0;
	} else {
		LOG_WARN("mount_easyfs: mount failed");
		brelse(b);
		// panic("mount_easyfs: mount failed");
		return -1;
	}
};

int easyfs_mount_root()
{
	if (!easyfs_root_mounted) {
		if (easyfs_mount() < 0)
			return -1;
		LOG_TRACE("mount_easyfs: mount root success!");
	}
	easyfs_root_mounted = 1;
	vfs_root = easyfs_get_vfs_inode(SUPER_INUM);

	sb.root = vfs_root;
	sb.dev = EASYFS_DEV;
	sb.block_size = BSIZE;
	sb.magic = EASYFS_MAGIC;
	sb.private_data = &easyfs_root_fs;

	return 0;
}

struct disk_super_block *easyfs_get_root_fs()
{
	if (!easyfs_root_mounted) {
		return 0;
	}
	return &easyfs_root_fs;
}

struct super_block *easyfs_get_root_sb()
{
	if (!easyfs_root_mounted) {
		return 0;
	}

	return &sb;
}
