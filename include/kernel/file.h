#ifndef FV_KERNEL_FILE_H
#define FV_KERNEL_FILE_H

#include "kernel/fs.h"

struct open_path {
	struct vfs_inode *start;
	char path[PATH_MAX];
};

#endif
