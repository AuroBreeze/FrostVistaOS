#ifndef __TMPFS_H_
#define __TMPFS_H_

#include "kernel/types.h"

#define TMPFS_MAGIC 0x011
#define TMPFS_DEV 0xb11

#define TMPFS_IBIT_NUM 5
#define TMPFS_DAIT_NUM 5
#define TMPFS_INO_NUM 12
#define TMPFS_DBLK_NUM 12

#define TMPFS_ADDR_TOT                                                         \
	(TMPFS_IBIT_NUM + TMPFS_DAIT_NUM + TMPFS_INO_NUM + TMPFS_DBLK_NUM)

struct tmpfs_superblock {
	uint64 magic;
	uint64 dev;
	// Bitmap page pointers: 3 direct entries, 1 single-indirect root,
	// and 1 double-indirect root.
	// record the page address where the bitmap is stored
	uint64 ibitmap[TMPFS_IBIT_NUM];
	uint64 dbitmap[TMPFS_DAIT_NUM];
	// 10 direct inode addresses, 1 single-indirect root, and 1
	// double-indirect root. record the inode addresses that are collected
	uint64 inode_collected[TMPFS_INO_NUM];
	uint64 data_collected[TMPFS_DBLK_NUM];
};

// sizeof(struct tmpfs_inode) = 32B
struct tmpfs_inode {
	uint16 type;
	uint16 nlinks;
	uint64 size;
	// record the page address where the data is stored
	// 10 direct data block addresses, 1 single-indirect root, and 1
	// double-indirect root.
	uint64 blocks[TMPFS_DBLK_NUM];
	uint32 padding[3]; // align to 32B
};

// sizeof(struct tmpfs_dir_entry) = 64B
struct tmpfs_dir_entry {
	uint64 inode_num;
	char name[56];
};

#endif
