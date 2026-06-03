#ifndef __EASYFS_H__
#define __EASYFS_H__

#include "kernel/fs.h"
#include "kernel/types.h"

#define SUPER_BLOCK 1 // super block

#define SUPER_INUM 0  // super block inode number
#define INOBLK_BMIP 2 // inode bitmap
#define DABLK_BMIP 3  // data block bitmap
#define INODE_BLOCK 4 // inode block
#define DATA_BLOCK 11 // data block
#define TOTAL_BLOCKS 1000

#define EASYFS_MAGIC 0x0B8EE2E0
#define EASYFS_DEV 0

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

extern struct super_block superblock;
// private data
struct easyfs_inode_info {
	uint32 blocks[12];
};

// super block private data
struct easyfs_super_info {
	uint32 total_blk;  // total number of blocks
	uint32 ibmip;	   // inode bitmap area
	uint32 dbmit;	   // data bitmap area
	uint32 ino_area;   // inode area
	uint32 datea_area; // data area
};

// mount.c
// int *easyfs_mount(struct disk_super_block *dsb);
int easyfs_mount_root();
struct disk_super_block *easyfs_get_root_fs();
struct super_block *easyfs_get_root_sb();

// alloc.c
uint32 balloc(uint32 dev);
void bfree(uint32 dev, uint32 block_num);
uint bmap(struct vfs_inode *ip, uint32 block_num);

// inode.c
struct vfs_inode *easyfs_fill_vfs_inode(uint32 ino, struct disk_inode *inode,
					uint8 file_type);
struct vfs_inode *ialloc(uint32 dev);
void iupdate(struct vfs_inode *ip);
struct vfs_inode *create(char *path, short type);
int easyfs_vfs_create(struct vfs_inode *dir, char *path, int mode);

// fs.c
int easyfs_read_inode(struct vfs_inode *ip, int user_dst, uint64 dst,
		      uint32 off, uint32 size);
int easyfs_write_inode(struct vfs_inode *ip, int user_src, uint64 src,
		       uint32 off, uint32 size);
void itrunc(struct vfs_inode *ip);
void easyfs_ilock(struct vfs_inode *ip);
void easyfs_iunlock(struct vfs_inode *ip);
void easyfs_iunlockput(struct vfs_inode *ip);
static struct vfs_inode *easyfs_namex(char *path, int nameiparent, char *name);
struct vfs_inode *easyfs_namei(char *path);
struct vfs_inode *easyfs_nameiparent(char *path, char *name);
int dirlink(struct vfs_inode *dp, char *name, uint inum);
struct vfs_inode *easyfs_get_vfs_inode(uint32 ino);

// dir.c
struct vfs_inode *easyfs_vfs_lookup(struct vfs_inode *ip, char *name,
				    uint32 *offset);

#endif
