#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"

#define LOG_MODULE " VFS"

struct vfs_inode *vfs_root;
static struct vfs_mount mounts[VFS_MAX_MOUNTS] = {0};
static struct vfs_inode early_root;

static struct vfs_inode *early_root_lookup(struct vfs_inode *dir, char *name,
					   uint32 *offset)
{
	(void) dir;
	(void) name;
	(void) offset;
	return 0;
}

static struct vfs_inode_ops early_root_ops = {
	.lookup = early_root_lookup,
};

/**
 * vfs_lookup_mount - Look up a mounted filesystem under a parent inode
 *
 * Match the pair (parent, name) recorded by vfs_mount_at(). The returned inode
 * is the root inode of the mounted filesystem, and the caller continues path
 * walking from that root.
 *
 * Return: mounted root inode on success, 0 if no mount exists
 * */
static struct vfs_inode *vfs_lookup_mount(struct vfs_inode *parent, char *name)
{
	for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
		if (mounts[i].parent == parent &&
		    namecmp(mounts[i].name, name) == 0) {
			return mounts[i].root;
		}
	}
	return 0;
}

/**
 * vfs_lookup_at - Look up a path from a starting inode
 *
 * Walk each path component from node. For every component, VFS checks the mount
 * table first, then falls back to the current inode's filesystem lookup
 * operation. This keeps mounted filesystems visible without requiring the
 * backing filesystem to contain or know about the mounted tree.
 *
 * Return: target inode on success, 0 if any component cannot be resolved
 * */
struct vfs_inode *vfs_lookup_at(struct vfs_inode *node, char *path)
{
	char name[PATH_MAX] = {0};
	struct vfs_inode *current = node;
	while ((path = skipelem(path, name)) != 0) {
		if (!(current->type & VFS_DIR))
			return 0;

		struct vfs_inode *next = vfs_lookup_mount(current, name);

		if (next == 0 &&
		    (current->ops == 0 || current->ops->lookup == 0))
			return 0;
		if (next == 0)
			next = current->ops->lookup(current, name, 0);
		if (next == 0)
			return 0;

		current = next;
	}
	return current;
}

/**
 * vfs_mount_at - Mount a filesystem root below an already resolved parent
 *
 * Register root at parent/name in the VFS mount table. name must be a single
 * path component: non-empty, shorter than DIRSIZ, and without '/'. A duplicate
 * mount at the same parent/name pair is rejected.
 *
 * Return: 0 on success, -1 on invalid arguments or a full mount table
 * */
int vfs_mount_at(struct vfs_inode *parent, char *name, struct vfs_inode *root)
{
	if (name == 0 || root == 0 || parent == 0)
		return -1;
	if (name[0] == '\0' || name[0] == '/')
		return -1;
	int namelen = 0;
	for (int i = 0; name[i] != '\0'; i++) {
		if (name[i] == '/')
			return -1;
		namelen++;
	}
	if (namelen >= DIRSIZ)
		return -1;
	for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
		if (mounts[i].parent == parent &&
		    namecmp(mounts[i].name, name) == 0) {
			return -1;
		}
	}
	for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
		if (mounts[i].root == 0) {
			strncpy(mounts[i].name, name, DIRSIZ);
			mounts[i].name[DIRSIZ - 1] = '\0';
			mounts[i].parent = parent;
			mounts[i].root = root;
			return 0;
		}
	}
	return -1;
}

/**
 * vfs_mount_fs - Mount a filesystem root at an absolute path
 *
 * Context: Split path into parent path and final component, resolve the parent from
 * vfs_root, then register the mount with vfs_mount_at(). Trailing slashes are
 * ignored, but mounting over "/" is not supported by this helper.
 *
 * Return: 0 on success, -1 if the path is invalid or the parent cannot mount
 * */
int vfs_mount_fs(char *path, struct vfs_inode *root)
{
	if (path == 0 || root == 0 || path[0] != '/')
		return -1;
	if (strlen(path) >= PATH_MAX)
		return -1;

	// Work on a private copy because splitting the path below writes '\0'
	// into the buffer. Callers may pass string literals such as "/dev".
	char fullpath[PATH_MAX];
	strncpy(fullpath, path, PATH_MAX);
	fullpath[PATH_MAX - 1] = '\0';

	// Treat "/dev/" and "/dev" as the same mount point, while keeping
	// the root path "/" intact so it can be rejected explicitly below.
	int len = (int) strlen(fullpath);
	while (len > 1 && fullpath[len - 1] == '/') {
		fullpath[--len] = '\0';
	}

	if (len <= 1)
		return -1;

	// Find the final path component. For "/mnt/ext4", fullpath still
	// contains the whole string and name will point at "ext4".
	char *name = fullpath + len - 1;
	while (name > fullpath && *(name - 1) != '/') {
		name--;
	}

	struct vfs_inode *parent;
	if (name == fullpath + 1) {
		// Mount directly under root, for example "/dev".
		parent = vfs_root;
	} else {
		// Split "/mnt/ext4" into parent path "/mnt" and name "ext4".
		*(name - 1) = '\0';
		parent = vfs_namei(fullpath);
	}

	// The mount table stores resolved (parent inode, name) pairs, not full
	// path strings. Future lookups hit this entry one component at a time.
	return vfs_mount_at(parent, name, root);
}

/**
 * vfs_namei - Look up path in the root and return the inode
 *
 * Only absolute paths are accepted. Relative lookup should use vfs_lookup_at()
 * with an explicit starting inode instead.
 *
 * Return: target inode on success, 0 if the path is invalid or not found
 * */
struct vfs_inode *vfs_namei(char *path)
{
	if (path == 0)
		return 0;

	if (*path != '/') {
		LOG_WARN("vfs_namei: path must start with /");
		return 0;
	}

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

void vfs_init()
{
	memset(&early_root, 0, sizeof(early_root));
	strcpy(early_root.name, "/");
	early_root.count = 1;
	early_root.nlinks = 1;
	early_root.type = VFS_DIR;
	early_root.ops = &early_root_ops;
	initsleeplock(&early_root.lock, "vfs root");
	vfs_root = &early_root;

#ifdef CONFIG_FS_DEVTMPFS
	extern void devtmpfs_init();
	extern struct vfs_inode *devtmpfs_root();
	devtmpfs_init();
	vfs_mount_fs("/dev", devtmpfs_root());
#endif
}
