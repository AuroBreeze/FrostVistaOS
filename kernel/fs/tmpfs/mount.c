#include "kernel/log.h"
#include "tmpfs.h"

static struct tmpfs_superblock tmpfs_root_fs;
static int tmpfs_root_mounted = 0;
struct vfs_inode *vfs_root;

int tmpfs_mount_root()
{
	if (tmpfs_root_mounted)
		LOG_ERROR("tmpfs had mounted");

	tmpfs_root_fs.magic = TMPFS_MAGIC;
	tmpfs_root_mounted = 1;
}
