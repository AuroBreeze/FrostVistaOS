#ifndef __EASYFS_H__
#define __EASYFS_H__

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

extern struct super_block superblock;
// private data
struct easyfs_inode_info {
	uint32 dev;
	uint32 type;
	uint32 valid;
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

#endif
