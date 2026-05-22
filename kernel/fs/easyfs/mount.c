// mount easyfs
#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/easyfs.h"
#include "kernel/fs.h"
#include "kernel/log.h"

struct super_block superblock = {0};

void show_root();

struct super_block *mount_easyfs(void)
{
	struct buf *b = bread(EASYFS_DEV, SUPER_BLOCK);
	struct disk_super_block *dsb = (struct disk_super_block *) b->data;
	if (dsb->magic == EASYFS_MAGIC) {
		LOG_TRACE("mount_easyfs: Magic number 0x0B8EE2E0 matched!");
	} else {
		LOG_ERROR("mount_easyfs: mount failed");
		panic("mount_easyfs: mount failed");
	}
	superblock.magic = dsb->magic;
	superblock.block_size = BSIZE;

	superblock.private_data = (struct easyfs_super_info *) kalloc();
	struct easyfs_super_info *info = superblock.private_data;

	struct vfs_inode *ip = get_inode(EASYFS_DEV, SUPER_INUM);
	// Get root inode data and acquire sleeplock
	ilock(ip);

	superblock.root = ip;
	strcpy(ip->name, "/");

	info->total_blk = TOTAL_BLOCKS;
	info->ibmip = INOBLK_BMIP;
	info->dbmit = DABLK_BMIP;
	info->ino_area = INODE_BLOCK;
	info->datea_area = DATA_BLOCK;

	// release sleeplock
	iunlock(ip);

	show_root();
	return &superblock;
};

void show_root()
{
	struct vfs_inode *root = superblock.root;
	struct disk_dir_entry *de = (struct disk_dir_entry *) kalloc();
	if (readi(root, 0, (uint64) de, 0, root->size) != root->size) {
		LOG_ERROR("read root error");
	}

	for (int i = 0; i < root->size / sizeof(struct disk_dir_entry); i++) {
		LOG_DEBUG("root: %s inode: %d", de[i].name, de[i].inode_num);
	}
	kfree(de);
}
