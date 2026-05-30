#include "driver/hal_console.h"
#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "core/proc.h"

struct vfs_inode *vfs_root;
extern struct vfs_inode_ops root_ops;

/**
 * vfs_lookup: Look up a path in the path and return the inod
 * */
struct vfs_inode *vfs_lookup_at(struct vfs_inode *node, char *path)
{
	char name[PATH_MAX] = {0};
	struct vfs_inode *current = node;
	while ((path = skipelem(path, name)) != 0) {
		if (!(current->type & VFS_DIR))
			return 0;

		struct vfs_inode *next = current->ops->lookup(current, name, 0);

#ifdef CONFIG_FS_DEVTMPFS
		if (next == 0 && current == vfs_root &&
		    strcmp(name, "dev") == 0) {
			extern struct vfs_inode *devtmpfs_root();
			next = devtmpfs_root();
		}
#endif

		if (next == 0)
			return 0;
		current = next;
	}
	return current;
}

/**
 * vfs_namei - Look up path in the root and return the inode
 * */
struct vfs_inode *vfs_namei(char *path)
{
	if (path == 0)
		return 0;

	return vfs_lookup_at(vfs_root, path);
}

/**
 * vfs_read_at - Read the contents of the file pointed to by node
 * */
int vfs_read_at(struct vfs_inode *node, uint64 off, uint8 *dst, uint32 size)
{
	if (node == 0 || node->default_f_ops == 0 ||
	    node->default_f_ops->read == 0) {
		return -1;
	}

	struct file f = {0};
	f.node = node;
	f.offset = off;
	f.readable = 1;
	f.writable = 0;
	f.ref_count = 1;

	return node->default_f_ops->read(&f, dst, size);
}

/**
 * vfs_ilock - only acquiresleep lock for the vfs inode
 * */
void vfs_ilock(struct vfs_inode *ip)
{
	if (ip == 0 || ip->count < 1)
		panic("vfs ilock");

	acquiresleep(&ip->lock);
}

/**
 * vfs_iunlock - only releasesleep lock for the vfs inode
 * */
void vfs_iunlock(struct vfs_inode *ip)
{
	if (ip == 0 || !holdingsleep(&ip->lock) || ip->count < 1)
		panic("vfs iunlock");

	releasesleep(&ip->lock);
}

void vfs_iput(struct vfs_inode *node)
{
	if (node == 0)
		return;

	if (node->sb != 0 && node->sb->ops != 0 &&
	    node->sb->ops->destroy_inode != 0) {
		node->sb->ops->destroy_inode(node);
		return;
	}

	LOG_WARN("kfree iput: %s", node->name);
	// fallback for temporary vnode backends
	if (node != vfs_root) {
		if (node->private_data)
			kfree(node->private_data);
		kfree(node);
	}
}

struct vfs_file_ops uart_ops;

struct vfs_inode *create_vfs_inode(char *name, uint32 flags)
{
	struct vfs_inode *node = kalloc();
	if (!node)
		return 0;

	strcpy(node->name, name);
	node->type = (short) flags;

	// test ops
	node->default_f_ops = &uart_ops;

	return node;
}

struct vfs_inode *dev_dir;
struct vfs_inode *tty_file;

// For testing purposes
struct vfs_inode *mock_finddir(struct vfs_inode *node, char *name,
			       uint32 *offset)
{
	(void) offset;

	if (node == vfs_root && strcmp(name, "dev") == 0) {
		return dev_dir;
	}
	if (node == dev_dir && strcmp(name, "tty") == 0) {
		return tty_file;
	}
	return 0; // Not found
}

struct vfs_inode_ops default_mock_ops = {.lookup = mock_finddir};

void vfs_init()
{
	vfs_root = create_vfs_inode("/", VFS_DIR);
	vfs_root->ops = &default_mock_ops;

#ifdef CONFIG_FS_DEVTMPFS
	extern void devtmpfs_init();
	devtmpfs_init();
#endif

	dev_dir = create_vfs_inode("dev", VFS_DIR);
	dev_dir->ops = &default_mock_ops;

	tty_file = create_vfs_inode("tty", VFS_FILE);
	tty_file->default_f_ops = &uart_ops;

	// At this point, the logical tree structure has been established: / ->
	// dev -> tty
}

void test_vfs()
{
	LOG_INFO("Test vfs");
	struct vfs_inode *node = vfs_lookup_at(vfs_root, "/dev/tty");
	if (node) {
		LOG_INFO("Success find node: %s", node->name);
	} else {
		LOG_ERROR("Fail to find node");
	}
}

int uart_vfs_write(struct file *, uint8 *buffer, uint32 size)
{
	for (uint32 i = 0; i < size; i++) {
		hal_console_putc(buffer[i]);
	}
	return (int) size;
}

// PERF: The current UART read operation uses a polling method and reads until
// the buffer is full. For the subsequent shell implementation, an
// interrupt-based method will be required, and operations such as line breaks
// will need to be handled separately.
int uart_vfs_read(struct file *, uint8 *buffer, uint32 size)
{
	int count = 0;
	for (uint32 i = 0; i < size; i++) {
		int c;
		while ((c = hal_console_getc()) <= 0) {
			yield();
		}

		buffer[i] = (uint8) c;
		count++;

		if (buffer[i] == '\r' || buffer[i] == '\n')
			break;
	}
	return count;
}

struct vfs_file_ops uart_ops = {
    .read = uart_vfs_read, .write = uart_vfs_write, .readdir = 0, .close = 0};
