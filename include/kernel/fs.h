#ifndef __FS_H_
#define __FS_H_

#include "kernel/sleeplock.h"
#include "kernel/stat.h"

#define VFS_DIR 0x0001
#define VFS_FILE 0x0010
#define VFS_DEV 0x0011

#define DIRSIZ 14
#define PATH_MAX 128

#define NDIRECT 10
#define NINDIRECT (BSIZE / sizeof(uint32))
#define NDINDIRECT (NINDIRECT * NINDIRECT)
#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT)
#define SINDIRECT_INDEX NDIRECT
#define DINDIRECT_INDEX (NDIRECT + 1)

struct super_block;
struct file;

// Directory entry
struct vfs_dirent {
	char name[28];
	uint32 ino;
};

#define VFS_MAX_MOUNTS 8
struct vfs_mount {
	char name[DIRSIZ];
	struct vfs_inode *root;
	struct vfs_inode *parent;
};

/**
 * vfs_inode_ops: Operations for a VFS node
 * */
struct vfs_inode_ops {
	struct vfs_inode *(*lookup)(struct vfs_inode *dir, char *name,
				    uint32 *offset);
	int (*create)(struct vfs_inode *dir, char *name, int mode);
	int (*link)(struct vfs_inode *old_node, struct vfs_inode *dir,
		    char *name);
	int (*unlink)(struct vfs_inode *dir, char *name);
	int (*mkdir)(struct vfs_inode *dir, char *name, int mode);
	int (*rmdir)(struct vfs_inode *dir, char *name);
	int (*rename)(struct vfs_inode *old_dir, char *old_name,
		      struct vfs_inode *new_dir, char *new_name);
	int (*stat)(struct vfs_inode *node, struct stat *st);
	int (*truncate)(struct vfs_inode *node, uint64 size);
};

struct vfs_file_ops {
	int (*read)(struct file *f, uint8 *buffer, uint32 size);
	int (*write)(struct file *f, uint8 *buffer, uint32 size);
	int (*readdir)(
	    struct file *f,
	    struct vfs_dirent *dirent); // `readdir` requires an offset, so
					// it is placed on the `file` side
	int (*lseek)(struct file *f, int64 offset, int whence);
	int (*close)(struct file *f);
};

struct vfs_inode {
	char name[16];
	uint32 ino;    // Inode number
	uint32 count;  // Reference count
	uint32 nlinks; // Number of hard links
	struct super_block *sb;
	struct sleeplock lock;
	struct vfs_inode_ops *ops; // pointer to the operations of the node
	struct vfs_file_ops *default_f_ops;

	short type;	    // type of the node
	uint64 size;	    // size of the node
	void *private_data; // Pointer to specific data

	// double linked list that supports LRU inode cache
	struct vfs_inode *next;
	struct vfs_inode *prev;
};

#define PIPE_BUF_SIZE 512
struct pipe {
	struct spinlock lock;
	char buf[PIPE_BUF_SIZE];
	uint nread;  // The number of bytes that have been read
	uint nwrite; // The number of bytes that have been written
	int readable;
	int writable;
};

enum file_type { FILE_NONE, FILE_VFS_NODE, FILE_PIPE };

struct file {
	enum file_type type;
	int ref_count; // Reference count (used by the `dup` system call)
	uint64 offset;
	uint8 readable;
	uint8 writable;
	uint8 append;

	struct vfs_file_ops *f_ops;
	struct vfs_inode *node; // Points to the corresponding VFS node
	struct pipe *pipe;
};

struct vfs_superblock_ops {
	struct vfs_inode *(*alloc_inode)(struct super_block *sb);
	void (*destroy_inode)(struct vfs_inode *inode);
	void (*write_super)(struct super_block *sb);
};

/**
 * super_block: Super block
 * */
struct super_block {
	uint32 magic;	   // magic number: supposed to be 0x0B8EE2E0
	uint32 dev;	   // device id
	uint32 block_size; // block size
	struct vfs_superblock_ops *ops;
	struct vfs_inode *root; // root of the filesystem
	void *private_data;	// Pointer to specific data
};

#endif
