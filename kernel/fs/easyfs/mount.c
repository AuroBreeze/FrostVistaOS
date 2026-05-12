// mount easyfs
#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/easyfs.h"
#include "kernel/fs.h"
#include "kernel/log.h"

struct super_block superblock = {0};

void show_root();
int creat_dev_tty();

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

	// inode area
	// struct buf *ino_buf = bread(0, 3);
	// struct disk_inode *ino = (struct disk_inode *) ino_buf->data;

	struct vfs_inode *ip = get_inode(EASYFS_DEV, SUPER_INUM);
	// Get root inode data and acquire sleeplock
	ilock(ip);

	superblock.root = ip;
	strcpy(ip->name, "/");

	LOG_TRACE("mount_easyfs: root: %s", ip->name);
	LOG_TRACE("mount_easyfs: root inode: %d", ip->ino);
	LOG_TRACE("mount_easyfs: root size: %d", ip->size);
	LOG_TRACE("mount_easyfs: root type: %d", ip->type);
	LOG_TRACE("mount_easyfs: root nlinks: %d", ip->nlinks);

	// release sleeplock
	iunlock(ip);

	show_root();
	creat_dev_tty();
	return &superblock;
};

void show_root()
{
	struct vfs_inode *root = superblock.root;
	// FIXME: Using `kalloc` will cause a memory leak
	struct disk_dir_entry *de = (struct disk_dir_entry *) kalloc();
	if (readi(root, 0, (uint64) de, 0, root->size) != root->size) {
		LOG_ERROR("read root error");
	}

	for (int i = 0; i < root->size / sizeof(struct disk_dir_entry); i++) {
		LOG_DEBUG("root: %s inode: %d", de[i].name, de[i].inode_num);
	}
}

int creat_dev_tty()
{
	struct vfs_inode *ip;
	struct vfs_inode *tty;

	// Create /dev
	if ((ip = create("/dev", VFS_DIR)) == 0) {
		LOG_ERROR("create /dev error");
		return -1;
	}
	releasesleep(&ip->lock);

	if ((tty = create("/dev/tty", VFS_DEV)) == 0) {
		LOG_ERROR("create /dev/tty error");
		return -1;
	}
	releasesleep(&tty->lock);
	return 0;
}
