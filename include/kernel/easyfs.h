#ifndef __EASYFS_H__
#define __EASYFS_H__

#include "kernel/types.h"

// private data
struct easyfs_inode_info {
  uint32 dev;
  uint32 type;
  uint32 blocks[12];
};

#endif
