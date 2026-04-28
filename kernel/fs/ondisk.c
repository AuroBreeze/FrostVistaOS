#include "kernel/types.h"

// Disk Superblock (e.g., exactly 64 bytes)
struct disk_super_block {
  uint32 magic; // Must be 0x0B8EE2E0 (BREEZE-0)
  uint32 total_blocks;
  uint32 inode_area_start;
  uint32 inode_area_end;
  uint32 ibitmap_area_start;
  uint32 ibitmap_area_end;
  uint32 dbitmap_area_start;
  uint32 dbitmap_area_end;
  uint32 data_area_start;
  uint32 data_area_end;
  uint32 padding[6]; // align to 64
};

// Disk Inode (e.g., exactly 64 bytes)
struct disk_inode {
  uint16 type;       // File or directory
  uint16 nlinks;     // Number of hard links
  uint32 size;       // Size in bytes
  uint32 blocks[12]; // Block numbers where data is stored
  uint32 padding[2]; // align to 64
};
