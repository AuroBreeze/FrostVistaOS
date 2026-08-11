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

// mix.c
struct vfs_inode *mix_vfs_lookup(struct vfs_inode *dir, char *name,
				 uint32 *offset);
int mix_vfs_readdir(struct file *f, struct vfs_dirent *dirent);
int mix_vfs_create(struct vfs_inode *dir, char *name, int mode);
int mix_vfs_mkdir(struct vfs_inode *dir, char *name, int mode);

#endif
