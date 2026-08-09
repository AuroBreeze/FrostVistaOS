#include "kernel/mm/kmalloc.h"
#define LOG_MODULE "TMPFS"

#include "asm/mm.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "tmpfs.h"

static struct tmpfs_dir_entry *root = 0;
extern struct vfs_inode *vfs_root;
static struct super_block sb = {0};

static int tmpfs_inited = 0;
static int tmpfs_root_mounted = 0;
static int tmpfs_mounted = 0;

static void tmpfs_init(void)
{
	if (tmpfs_inited) {
		return;
	}

	root = kmalloc(sizeof(struct tmpfs_dir_entry));
	if (!root) {
		return;
	}

	root->child = 0;
	root->next = 0;
	root->inode = 0;

	tmpfs_inited = 1;
}

int tmpfs_mount_root()
{
	if (!tmpfs_inited) {
		tmpfs_init();
	}
	if (tmpfs_root_mounted) {
		LOG_WARN("tmpfs already mounted");
		return -1;
	}

	tmpfs_root_mounted = 1;

	struct tmpfs_inode *root_inode = kmalloc(sizeof(struct tmpfs_inode));
	if (!root_inode) {
		return -1;
	}

	root_inode->type = VFS_DIR;
	root_inode->nlinks = 1;
	root_inode->dir = root;
	root_inode->size = 0;

	vfs_root = tmpfs_fill_vfs_inode(TMPFS_ROOT_INO, root_inode, VFS_DIR);

	sb.root = vfs_root;
	sb.dev = TMPFS_DEV;
	sb.block_size = PGSIZE;
	sb.magic = TMPFS_MAGIC;
	sb.private_data = root;

	return 0;
}

struct tmpfs_dir_entry *tmpfs_get_root_dir_entry()
{
	if (!tmpfs_root_mounted) {
		return 0;
	}

	return root;
}

struct super_block *tmpfs_get_root_sb()
{
	if (!tmpfs_root_mounted) {
		return 0;
	}
	return &sb;
}
