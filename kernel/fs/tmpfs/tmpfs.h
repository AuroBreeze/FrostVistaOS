#ifndef __TMPFS_H_
#define __TMPFS_H_

#include "kernel/fs.h"
#include "kernel/types.h"
#include "asm/mm.h"

#define TMPFS_DEV 0x0b11
#define TMPFS_MAGIC 0x20202020
#define TMPFS_ROOT_INO 1

#define BLOCKS 12
#define BLOCK_SIZE PGSIZE

#define TMPFS_NDIRECT 10
#define TMPFS_NINDIRECT (PGSIZE / sizeof(uint64)) /* 512 */
#define TMPFS_NDINDIRECT (TMPFS_NINDIRECT * TMPFS_NINDIRECT)
#define TMPFS_MAXFILE (TMPFS_NDIRECT + TMPFS_NINDIRECT + TMPFS_NDINDIRECT)

struct tmpfs_inode {
	uint16 type;
	uint16 nlinks;
	uint32 ino;
	uint64 size;
	struct tmpfs_dir_entry *dir;
	// record the page address where the data is stored
	// 10 direct data block addresses, 1 single-indirect root, and 1
	// double-indirect root.
	uint64 blocks[BLOCKS];
};

struct tmpfs_dir_entry {
	struct tmpfs_inode *inode; /* inode address */
	char name[DIRSIZ];
	uint8 type;

	// Left-child, right-sibling representation
	struct tmpfs_dir_entry *next;
	struct tmpfs_dir_entry *child;
};

// mount.c
struct tmpfs_dir_entry *tmpfs_get_root_dir_entry();
struct super_block *tmpfs_get_root_sb();
struct vfs_inode *tmpfs_root();

// inode.c
struct vfs_inode *tmpfs_vfs_lookup(struct vfs_inode *dir, char *name,
				   uint32 *offset);
struct vfs_inode *tmpfs_fill_vfs_inode(uint32 ino, struct tmpfs_inode *inode,
				       uint8 file_type);
int tmpfs_vfs_create(struct vfs_inode *dir, char *name, int type);
void destroy_inode(struct vfs_inode *inode);
int tmpfs_vfs_readdir(struct file *f, struct vfs_dirent *dirent);
int tmpfs_vfs_mkdir(struct vfs_inode *dir, char *name, int mode);
int tmpfs_vfs_read(struct file *f, uint8 *buffer, uint32 size);
int tmpfs_vfs_write(struct file *f, uint8 *buffer, uint32 size);
int tmpfs_vfs_stat(struct vfs_inode *node, struct stat *st);

struct vfs_inode_ops *get_vfs_inode_ops();
struct vfs_file_ops *get_vfs_file_ops();
struct vfs_superblock_ops *get_vfs_superblock_ops();

#endif
