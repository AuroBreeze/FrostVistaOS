#ifndef __ICACHE_H__
#define __ICACHE_H__

#include "kernel/spinlock.h"
#include "kernel/fs.h"

#define NINODES 50

struct inode_cache {
	struct spinlock lock;

	vfs_inode_t head; // double linked list
	vfs_inode_t inodes[NINODES];
};

extern struct inode_cache icache;

struct vfs_inode *get_inode(uint32 ino);
void icache_init(void);
void put_inode(struct vfs_inode *t);

#endif
