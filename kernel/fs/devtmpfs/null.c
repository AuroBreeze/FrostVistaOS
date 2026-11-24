#include "kernel/fs.h"

static int devtmpfs_null_read(struct file *f, uint8 *buffer, uint32 size)
{
	(void) f;
	(void) buffer;
	(void) size;
	return 0;
}

static int devtmpfs_null_write(struct file *f, uint8 *buffer, uint32 size)
{
	(void) f;
	(void) buffer;
	return size;
}

struct vfs_file_ops null_ops = {
    .read = devtmpfs_null_read,
    .write = devtmpfs_null_write,
};
