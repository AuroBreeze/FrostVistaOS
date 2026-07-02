#ifndef __TMPFS_H_
#define __TMPFS_H_

#include "kernel/fs.h"

#define offsetof(TYPE, MEMBER) ((uint64) & (((TYPE *) 0)->MEMBER))
// Because the member offset is only calculated at compile time and address 0 is
// not actually accessed.
#define container_of(ptr, type, member)                                        \
	({                                                                     \
		const typeof(((type *) 0)->member) *__mptr = (ptr);            \
		(type *) ((char *) __mptr - offsetof(type, member));           \
	})

#define TMPFS_DEV 0x111
#define TMPFS_MAGIC 0x01111111

struct tmpfs_superblock {
	// dirent pointer
	struct tmpfs_dirent *root;
	uint64 magic;
};

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

struct tmpfs_dirent {
	char name[DIRSIZ];
	uint32 ino;
	// PERF: Other data structures can be used later to speed up
	// indexing.

	// Doubly linked lists ensure that deletion does not cause the chain to
	// break.
	struct tmpfs_dirent *parent;
	struct tmpfs_inode *inode;
	// Intrusive data structures
	struct list_head list;
};

struct tmpfs_file {
	void *space; // Allocated by kalloc
	struct list_head list;
};

struct tmpfs_inode {
	short type; // type of the node
	union {
		struct {
			struct tmpfs_dirent *children;
			int count; // number of children
		} dir;
		struct {
			struct tmpfs_file *files;
			int count; // number of space
		} files;
	};
};

uint32 alloc_ino();
struct tmpfs_inode *tmpfs_get_root_inode();
struct tmpfs_dirent *tmpfs_get_root_dirent();

struct vfs_inode *tmpfs_vfs_lookup(struct vfs_inode *ip, char *name,
				   uint32 *offset);

// inode.c
void tmpfs_destroy_inode(struct vfs_inode *inode);
struct vfs_inode *tmpfs_fill_vfs_inode(uint32 ino, struct tmpfs_inode *tip,
				       struct tmpfs_dirent *de);

#endif
