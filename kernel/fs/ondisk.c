#include "kernel/bcache.h"
#include "kernel/log.h"
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

void test_read_img() {
  binit();
  // The moment of truth: Read Block 1 (where your SuperBlock lives)
  // Assume device 0 is your virtio disk
  LOG_DEBUG("[FS] Reading Block 1 (SuperBlock) from device 0");
  struct buf *b = bread(0, 1);
  LOG_DEBUG("[FS] Read Block 1 (SuperBlock) from device 0");
  // Cast the raw data to check the magic number
  // We just check the first 4 bytes for simplicity
  uint32 *magic_ptr = (uint32 *)b->data;

  if (*magic_ptr == 0x0B8EE2E0) {
    LOG_DEBUG("[FS] SUCCESS! Magic number 0x0B8EE2E0 matched!");
    LOG_DEBUG("[FS] FrostVistaOS has successfully mounted the root disk!");
  } else {
    LOG_DEBUG("[FS] MOUNT FAILED! Expected 0x0B8EE2E0 but got 0x%x",
              *magic_ptr);
  }

  // CRUCIAL: Always release the buffer!
  brelse(b);
}
