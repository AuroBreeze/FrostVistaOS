#ifndef __FS_H_
#define __FS_H_

#include "kernel/spinlock.h"
#define VFS_DIR 0x0001
#define VFS_FILE 0x0010

#include "kernel/defs.h"
#include "kernel/stat.h"

struct super_block;

// Disk Superblock (e.g., exactly 32 bytes)
struct disk_super_block {
	uint32 magic; // Must be 0x0B8EE2E0 (BREEZE-0)
	uint32 total_blocks;
	uint32 ibitmap_area_start;
	uint32 dbitmap_area_start;
	uint32 inode_area_start;
	uint32 data_area_start;
	uint32 padding[2]; // align to 32
};

// Disk Inode (e.g., exactly 64 bytes)
struct disk_inode {
	uint16 type;	   // File or directory
	uint16 nlinks;	   // Number of hard links
	uint32 size;	   // Size in bytes
	uint32 blocks[12]; // Block numbers where data is stored
	uint32 padding[2]; // align to 64
};

// A simple directory entry structure
struct disk_dir_entry {
	uint32 inode_num; // Inode number
	char name[28];	  // File/Directory name
};
/**
 * vfs_inode_ops: Operations for a VFS node
 * */
typedef struct vfs_inode_ops {
	struct vfs_inode *(*lookup)(struct vfs_inode *dir, char *name);
	int (*create)(struct vfs_inode *dir, char *name, int mode);
	int (*link)(struct vfs_inode *old_node, struct vfs_inode *dir,
		    char *name);
	int (*unlink)(struct vfs_inode *dir, char *name);
	int (*mkdir)(struct vfs_inode *dir, char *name, int mode);
	int (*rmdir)(struct vfs_inode *dir, char *name);
	int (*rename)(struct vfs_inode *old_dir, char *old_name,
		      struct vfs_inode *new_dir, char *new_name);
	int (*stat)(struct vfs_inode *node, struct stat *st);

} vfs_inode_ops_t;

typedef struct vfs_file_ops {
	int (*read)(struct file *f, uint8 *buffer, uint32 size);
	int (*write)(struct file *f, uint8 *buffer, uint32 size);
	int (*readdir)(
	    struct file *f,
	    struct vfs_dirent *dirent); // `readdir` requires an offset, so
					// it is placed on the `file` side
	int (*lseek)(struct file *f, int64 offset, int whence);
	int (*close)(struct file *f);
} vfs_file_ops_t;

typedef struct vfs_inode {
	char name[16];
	uint32 ino;    // Inode number
	uint32 count;  // Reference count
	uint32 nlinks; // Number of hard links
	struct super_block *sb;
	struct spinlock lock;
	vfs_inode_ops_t *ops; // pointer to the operations of the node
	vfs_file_ops_t *default_f_ops;

	short type;	    // type of the node
	uint64 size;	    // size of the node
	void *private_data; // Pointer to specific data

	// double linked list that supports LRU inode cache
	struct vfs_inode *next;
	struct vfs_inode *prev;
} vfs_inode_t;

typedef struct superblock_ops {
	struct vfs_inode *(*alloc_inode)(struct super_block *sb);
	void (*destroy_inode)(struct vfs_inode *inode);
	void (*write_super)(struct super_block *sb);
} superblock_ops_t;

typedef struct fs_ops {
	struct super_block *(*mount_fs)(void);
} fs_ops_t;

/**
 * super_block: Super block
 * */
typedef struct super_block {
	uint64 magic;	   // magic number: suppose to be 0x0B8EE2E0
	uint32 dev;	   // device id
	uint32 block_size; // block size
	struct superblock_ops *ops;
	struct vfs_inode *root; // root of the filesystem
	void *private_data;	// Pointer to specific data
} super_block_t;

uint readi(struct vfs_inode *ip, int user_dst, uint64 dst, uint32 off,
	   uint32 size);

#endif
