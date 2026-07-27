#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "tmpfs.h"

static struct tmpfs_superblock tmpfs_root_fs;
struct super_block tmpfs_sb = {0};
static int tmpfs_root_mounted = 0;
static int tmpfs_mounted = 0;
struct vfs_inode *vfs_root;

int init_address()
{
	uint64 addr;
	uint64 addrs[TMPFS_ADDR_TOT] = {0};
	int used = 0;

	for (; used < TMPFS_ADDR_TOT; used++) {
		if ((addr = (uint64) kalloc()) != 0) {
			addrs[used] = addr;
		} else {
			goto fail;
		}
	}

	for (int i = 0; i < TMPFS_IBIT_NUM; i++) {
		tmpfs_root_fs.ibitmap[i] = addrs[i];
	}
	for (int i = 0; i < TMPFS_DAIT_NUM; i++) {
		tmpfs_root_fs.dbitmap[i] = addrs[TMPFS_IBIT_NUM + i];
	}
	for (int i = 0; i < TMPFS_INO_NUM; i++) {
		tmpfs_root_fs.inode_collected[i] =
		    addrs[TMPFS_IBIT_NUM + TMPFS_DAIT_NUM + i];
	}
	for (int i = 0; i < TMPFS_DBLK_NUM; i++) {
		tmpfs_root_fs.data_collected[i] =
		    addrs[TMPFS_IBIT_NUM + TMPFS_DAIT_NUM + TMPFS_INO_NUM + i];
	}

	return 0;

fail:
	for (int i = 0; i < used; i++) {
		kfree((void *) addrs[i]);
	}
	return -1;
}

int tmpfs_mount()
{
	if (tmpfs_root_mounted)
		LOG_ERROR("tmpfs had mounted");

	tmpfs_root_fs.magic = TMPFS_MAGIC;
	tmpfs_root_fs.dev = TMPFS_DEV;

	if (init_address() < 0)
		return -1;

	tmpfs_mounted = 1;
	return 0;
}

int tmpfs_mount_root()
{
	tmpfs_mount();
	if (!tmpfs_mounted || tmpfs_root_mounted) {
		return -1;
	}
	// PERF: alloc a page for vfs_root but it's not necessary
	if ((vfs_root = kalloc()) == 0)
		return -1;
	vfs_root->dev = tmpfs_root_fs.dev;
	vfs_root->count = 1;
	vfs_root->nlinks = 1;
	vfs_root->type = VFS_DIR;

	tmpfs_sb.root = vfs_root;
	tmpfs_sb.block_size = 0x1000;
	tmpfs_sb.dev = tmpfs_root_fs.dev;
	tmpfs_sb.magic = tmpfs_root_fs.magic;

	vfs_root->sb = &tmpfs_sb;

	tmpfs_root_mounted = 1;
	return 0;
}

struct tmpfs_superblock *tmpfs_get_superblock()
{
	if (!tmpfs_mounted)
		return 0;

	return &tmpfs_root_fs;
}
