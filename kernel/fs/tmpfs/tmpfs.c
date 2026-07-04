#include "tmpfs.h"
#include "asm/mm.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/spinlock.h"

struct vfs_superblock_ops sb_ops = {
    .alloc_inode = 0,
    .destroy_inode = tmpfs_destroy_inode,
    .write_super = 0,
};

struct vfs_file_ops tmpfs_file_ops = {
    .read = tmpfs_vfs_read,
    .write = 0,
    .readdir = 0,
    .lseek = 0,
    .close = 0,
};

struct vfs_inode_ops tmpfs_inode_ops = {
    .lookup = tmpfs_vfs_lookup,
    .stat = 0,
    .create = tmpfs_vfs_create,
    .truncate = 0,
    .unlink = 0,
    .mkdir = 0,
};

extern struct vfs_inode *vfs_root;
static int tmpfs_root_mounted = 0;
struct super_block tmpfs_sb = {0};

static uint32 ino = 1;
struct spinlock tmpfs_lock = {0};

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

void init_list(struct list_head *head)
{
	head->next = head;
	head->prev = head;
}

struct vfs_inode *tmpfs_setup_root()
{
	strcpy(root.name, "/");
	init_list(&root.list);
	root_inode.type = VFS_DIR;

	// ---test---
	static struct tmpfs_dirent a = {0};
	strcpy(a.name, "a");
	a.ino = alloc_ino();
	a.parent = &root;
	static struct tmpfs_inode a_inode = {0};
	a_inode.type = VFS_FILE;
	static struct tmpfs_file files = {0};
	files.space = kalloc();
	if (files.space == 0)
		return 0;
	char *content = "hello world";
	memcpy(files.space, content, strlen(content));
	a_inode.size = strlen(content);
	a_inode.files.files = &files;
	init_list(&a_inode.files.files->list);
	a.inode = &a_inode;
	init_list(&a.list);
	// --- ---

	root_inode.dir.children = &a;
	root.inode = &root_inode;
	root.parent = 0;
	root.ino = alloc_ino();

	return tmpfs_fill_vfs_inode(root.ino, &root_inode, &root);
}

int tmpfs_mount_root()
{
	if (tmpfs_root_mounted == 1)
		return -1;

	initlock(&tmpfs_lock, "tmpfs_lock");

	tmpfs_sb.block_size = PGSIZE;
	tmpfs_sb.root = tmpfs_setup_root();
	tmpfs_sb.magic = TMPFS_MAGIC;
	tmpfs_sb.dev = TMPFS_DEV;
	tmpfs_sb.ops = &sb_ops;
	tmpfs_sb.private_data = &root; // struct tmpfs_dirent

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
