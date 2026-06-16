#include "driver/hal_console.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/spinlock.h"
#include "kernel/stat.h"

#define LOG_MODULE "DEVT"

static struct vfs_inode dev_root;
static struct vfs_inode_ops devtmpfs_ops;

#define DEVTMPFS_MAX_NODES 8
static struct vfs_inode dev_nodes[DEVTMPFS_MAX_NODES];
static int dev_node_count;
struct spinlock tty_lock = {.name = "tty_lock", .locked = 0, .cpu = 0};

static int devtmpfs_tty_write(struct file *, uint8 *buffer, uint32 size)
{
	acquire(&tty_lock);
	for (uint32 i = 0; i < size; i++) {
		hal_console_putc(buffer[i]);
	}
	release(&tty_lock);
	return (int) size;
}

static int devtmpfs_tty_read(struct file *, uint8 *buffer, uint32 size)
{
	if (size == 0)
		return 0;

	int c;
	while ((c = hal_console_getc()) <= 0) {
		yield();
	}

	buffer[0] = (uint8) c;
	return 1;
}

static struct vfs_file_ops tty_ops = {
    .read = devtmpfs_tty_read,
    .write = devtmpfs_tty_write,
};

static void devtmpfs_destroy_inode(struct vfs_inode *node)
{
	(void) node;
}

static struct vfs_superblock_ops devtmpfs_sb_ops = {
    .destroy_inode = devtmpfs_destroy_inode,
};

static struct super_block devtmpfs_sb = {
    .ops = &devtmpfs_sb_ops,
};

struct vfs_inode *devtmpfs_register(char *name, short type,
				    struct vfs_file_ops *f_ops)
{
	if (name == 0 || dev_node_count >= DEVTMPFS_MAX_NODES)
		return 0;
	for (int i = 0; i < dev_node_count; i++) {
		if (strcmp(dev_nodes[i].name, name) == 0)
			return &dev_nodes[i];
	}

	struct vfs_inode *node = &dev_nodes[dev_node_count++];
	memset(node, 0, sizeof(*node));
	strncpy(node->name, name, sizeof(node->name));
	node->name[sizeof(node->name) - 1] = '\0';
	node->count = 1;
	node->nlinks = 1;
	node->type = type;
	node->sb = &devtmpfs_sb;
	node->ops = &devtmpfs_ops;
	node->default_f_ops = f_ops;
	initsleeplock(&node->lock, "devtmpfs node");
	return node;
}

static struct vfs_inode *devtmpfs_lookup(struct vfs_inode *dir, char *name,
					 uint32 *offset)
{
	if (offset != 0) {
		*offset = 0;
	}

	if (dir == &dev_root) {
		for (int i = 0; i < dev_node_count; i++) {
			if (strcmp(dev_nodes[i].name, name) == 0)
				return &dev_nodes[i];
		}
	}

	return 0;
}

static int devtmpfs_stat(struct vfs_inode *node, struct stat *st)
{
	if (node == 0 || st == 0) {
		return -1;
	}

	st->addr = 0;
	st->type = node->type == VFS_DIR ? T_DIR : T_DEVICE;
	st->nlink = node->nlinks;
	st->size = node->size;
	return 0;
}

static struct vfs_inode_ops devtmpfs_ops = {
    .lookup = devtmpfs_lookup,
    .stat = devtmpfs_stat,
};

void devtmpfs_init(void)
{
	dev_node_count = 0;
	memset(dev_nodes, 0, sizeof(dev_nodes));

	memset(&dev_root, 0, sizeof(dev_root));
	strcpy(dev_root.name, "dev");
	dev_root.count = 1;
	dev_root.nlinks = 1;
	dev_root.type = VFS_DIR;
	dev_root.sb = &devtmpfs_sb;
	dev_root.ops = &devtmpfs_ops;
	initsleeplock(&dev_root.lock, "devtmpfs dir");

	devtmpfs_register("tty", VFS_DEV, &tty_ops);
}

struct vfs_inode *devtmpfs_root(void)
{
	return &dev_root;
}
