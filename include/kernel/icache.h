#ifndef FV_KERNEL_ICACHE_H
#define FV_KERNEL_ICACHE_H

#include "kernel/spinlock.h"
#include "kernel/fs.h"

#define NINODES 50

struct inode_cache {
	struct spinlock lock;

	struct vfs_inode head; // double linked list
	struct vfs_inode inodes[NINODES];
};

extern struct inode_cache icache;

#endif
