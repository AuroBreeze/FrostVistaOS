#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/stat.h"

extern struct vfs_file_ops uart_ops;

static struct vfs_inode dev_root;
static struct vfs_inode tty_node;

static struct vfs_inode *devtmpfs_lookup(struct vfs_inode *dir, char *name,
					 uint32 *offset)
{
	if (offset != 0) {
		*offset = 0;
	}

	if (dir == &dev_root && strcmp(name, "tty") == 0) {
		return &tty_node;
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
	memset(&dev_root, 0, sizeof(dev_root));
	strcpy(dev_root.name, "dev");
	dev_root.count = 1;
	dev_root.nlinks = 1;
	dev_root.type = VFS_DIR;
	dev_root.ops = &devtmpfs_ops;
	initsleeplock(&dev_root.lock, "devtmpfs dir");

	memset(&tty_node, 0, sizeof(tty_node));
	strcpy(tty_node.name, "tty");
	tty_node.count = 1;
	tty_node.nlinks = 1;
	tty_node.type = VFS_DEV;
	tty_node.ops = &devtmpfs_ops;
	tty_node.default_f_ops = &uart_ops;
	initsleeplock(&tty_node.lock, "devtmpfs tty");
}

struct vfs_inode *devtmpfs_root(void)
{
	return &dev_root;
}
