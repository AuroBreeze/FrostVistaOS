
#include "kernel/defs.h"
#define LOG_MODULE "TMPFS"

#include "kernel/arch/mm.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "tmpfs.h"

extern struct vfs_inode *vfs_root;

static struct tmpfs_dir_entry root_dir = {0};
static struct tmpfs_inode root_inode = {0};
static struct super_block sb = {0};
struct vfs_inode tmp_root = {0};

static int tmpfs_inited = 0;
static int tmpfs_root_mounted = 0;
static int tmpfs_mounted = 0;

static void tmpfs_init(void)
{
	if (tmpfs_inited) {
		return;
	}

	root_dir.child = 0;
	root_dir.next = 0;
	root_dir.inode = &root_inode;

	memset(&tmp_root, 0, sizeof(tmp_root));
	tmp_root.type = VFS_DIR;
	tmp_root.ino = TMPFS_ROOT_INO;
	tmp_root.nlinks = 1;
	tmp_root.count = 1;
	tmp_root.dev = TMPFS_DEV;
	tmp_root.sb = &sb;
	tmp_root.ops = get_vfs_inode_ops();
	tmp_root.default_f_ops = get_vfs_file_ops();
	tmp_root.private_data = &root_inode;
	tmp_root.size = 0;
	initsleeplock(&tmp_root.lock, "tmp_root");
	strcpy(tmp_root.name, "tmp");

	root_inode.type = VFS_DIR;
	root_inode.nlinks = 1;
	root_inode.ino = TMPFS_ROOT_INO;
	root_inode.dir = &root_dir;
	root_inode.size = 0;
	root_dir.inode = &root_inode;

	sb.root = &tmp_root;
	sb.dev = TMPFS_DEV;
	sb.block_size = PGSIZE;
	sb.magic = TMPFS_MAGIC;
	sb.private_data = &root_dir;
	sb.ops = get_vfs_superblock_ops();

	// tmpfs_init fully prepares the static root/superblock; with the
	// vfs_root = tmpfs_root() integration, tmpfs_mount_root() is no longer
	// called, so mark the root as mounted here.
	tmpfs_root_mounted = 1;

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

	// FIXME: An unconditional replacement occurs within `vfs_init`, leading
	// to an icache inode leak. Since the implementation and testing of
	// `tmpfs` are still ongoing—and the plan is to formally integrate with
	// the VFS and discontinue the use of the `early_root` node—the eventual
	// approach should be to remove `early_root` and establish `tmpfs` as
	// the initial `vfs_root`.
	// However, it is important to note that the `vfs_root` will still need
	// to be replaced later when the disk based filesystem is mounted care
	// must be taken during this process to avoid icache inode leaks.
	vfs_root = tmpfs_fill_vfs_inode(TMPFS_ROOT_INO, &root_inode, VFS_DIR);

	return 0;
}

struct tmpfs_dir_entry *tmpfs_get_root_dir_entry()
{
	if (!tmpfs_root_mounted) {
		return 0;
	}

	return &root_dir;
}

struct vfs_inode *tmpfs_root()
{
	if (!tmpfs_inited) {
		tmpfs_init();
	}

	return &tmp_root;
}

struct super_block *tmpfs_get_root_sb()
{
	if (!tmpfs_root_mounted) {
		return 0;
	}
	return &sb;
}
