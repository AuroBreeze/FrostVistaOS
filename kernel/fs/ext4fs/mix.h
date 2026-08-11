#ifndef __EXT4FS_MIX_H__
#define __EXT4FS_MIX_H__

#include "kernel/fs.h"

#define OVERLAY_HASH 32
#define OVERLAY_ENTRIES 64

struct overlay_entry {
	char name[DIRSIZ];
	struct vfs_inode *root;
	struct vfs_inode *parent;

	int whiteout;

	// hash bucket
	struct overlay_entry *next;
	int used;
};

#endif
