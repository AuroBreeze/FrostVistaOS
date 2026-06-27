#include "tmpfs.h"
#include "asm/mm.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/spinlock.h"

struct vfs_superblock_ops sb_ops = {
    .alloc_inode = 0,
    .destroy_inode = 0,
    .write_super = 0,
};

struct vfs_file_ops tmpfs_file_ops = {
    .read = 0,
    .write = 0,
    .readdir = 0,
    .lseek = 0,
    .close = 0,
};

struct vfs_inode_ops tmpfs_inode_ops = {
    .lookup = 0,
    .stat = 0,
    .create = 0,
    .truncate = 0,
    .unlink = 0,
    .mkdir = 0,
};

extern struct vfs_inode *vfs_root;
static int tmpfs_root_mounted = 0;
struct super_block tmpfs_sb = {0};

static uint32 ino = 1;
struct spinlock tmpfs_lock = {.name = "tmpfs_lock", .locked = 0, .cpu = 0};

static struct tmpfs_dirent root = {0};
static struct tmpfs_inode root_inode = {0};

uint32 alloc_ino()
{
	uint32 i;
	acquire(&tmpfs_lock);
	i = ino++;
	release(&tmpfs_lock);
	return i;
}

static void init_list(struct list_head *head)
{
	head->next = head;
	head->prev = head;
}

struct vfs_inode *tmpfs_setup_root()
{
	strcpy(root.name, "/");
	init_list(&root.list);
	root_inode.type = VFS_DIR;
	root_inode.dir.children = 0;

	root.inode = &root_inode;
	root.parent = 0;
	root.ino = alloc_ino();

	return tmpfs_fill_vfs_inode(root.ino, &root_inode, &root);
}

int tmpfs_mount_root()
{
	if (tmpfs_root_mounted == 1)
		return -1;

	tmpfs_sb.block_size = PGSIZE;
	tmpfs_sb.root = tmpfs_setup_root();
	tmpfs_sb.magic = TMPFS_MAGIC;
	tmpfs_sb.dev = TMPFS_DEV;
	tmpfs_sb.ops = &sb_ops;
	tmpfs_sb.private_data = &root; // struct tmpfs_diren

	vfs_root = tmpfs_sb.root;
	tmpfs_root_mounted = 1;
	return 0;
}

struct tmpfs_inode *tmpfs_get_root_inode()
{
	if (tmpfs_root_mounted == 0)
		return 0;
	return &root_inode;
}
struct tmpfs_dirent *tmpfs_get_root_dirent()
{
	if (tmpfs_root_mounted == 0)
		return 0;
	return &root;
}
