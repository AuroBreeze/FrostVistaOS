#include "kernel/types.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BLOCK_SIZE 4096
#define MAGIC_NUM 0x0B8EE2E0
#define TOTAL_BLOCKS 10000
#define VFS_DIR 0x0001
#define VFS_FILE 0x0010


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
  uint16 type;       // File or directory
  uint16 nlinks;     // Number of hard links
  uint32 size;       // Size in bytes
  uint32 blocks[12]; // Block numbers where data is stored
  uint32 padding[2]; // align to 64
};

// A simple directory entry structure
struct disk_dir_entry {
  uint32 inode_num; // Inode number
  char name[28];    // File/Directory name
};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: mkfs <image file>\n");
    return -1;
  }

  // open the image file for writing in binary mode
  FILE *img = fopen(argv[1], "wb");
  if (!img) {
    perror("Failed to open image file");
    return -1;
  }

  // fill the entire file with zeros
  uint8_t zero_block[BLOCK_SIZE] = {0};
  for (int i = 0; i < TOTAL_BLOCKS; i++) {
    fwrite(zero_block, BLOCK_SIZE, 1, img);
  }

  // initialize and write the super block
  struct disk_super_block super_block = {0};
  super_block.magic = MAGIC_NUM;
  super_block.total_blocks = TOTAL_BLOCKS;

  // Calculate layout:
  // Block 0: Boot block (unused here)
  // Block 1: SuperBlock
  // Block 2: Inode Bitmap
  // Block 3: Data Bitmap
  // Block 4 to 10: Inode Area (e.g., 7 blocks * 64 inodes/block = 448 inodes)
  // Block 11 to 9999: Data Area
  super_block.ibitmap_area_start = 2;
  super_block.dbitmap_area_start = 3;
  super_block.inode_area_start = 4;
  super_block.data_area_start = 11;

  fseek(img, 1 * BLOCK_SIZE, SEEK_SET);
  fwrite(&super_block, sizeof(super_block), 1, img);

  // setup Root Inode (Inode #0)
  struct disk_inode root_inode = {0};
  root_inode.type = VFS_DIR;
  root_inode.nlinks = 2; // . and .. pointer to self
  root_inode.size = 2 * sizeof(struct disk_dir_entry);
  root_inode.blocks[0] =
      super_block.data_area_start; // Assign the first data block

  // write Root Inode to the beginning of the Inode Area
  fseek(img, super_block.inode_area_start * BLOCK_SIZE, SEEK_SET);
  fwrite(&root_inode, sizeof(struct disk_inode), 1, img);

  // write root inode to the beginning of the inode area
  struct disk_dir_entry dot_entry[2] = {0};
  dot_entry[0].inode_num = 0; // pointer to self
  strcpy(dot_entry[0].name, ".");
  dot_entry[1].inode_num = 0; // pointer to self
  strcpy(dot_entry[1].name, "..");

  fseek(img, root_inode.blocks[0] * BLOCK_SIZE, SEEK_SET);
  fwrite(dot_entry, sizeof(struct disk_dir_entry), 2, img);

  // update bitmap
  zero_block[0] = 0x1; // mark block 0 as allocated
  // update inode bitmap
  fseek(img, super_block.ibitmap_area_start * BLOCK_SIZE, SEEK_SET);
  fwrite(zero_block, BLOCK_SIZE, 1, img);

  // update data bitmap
  fseek(img, super_block.dbitmap_area_start * BLOCK_SIZE, SEEK_SET);
  fwrite(zero_block, BLOCK_SIZE, 1, img);

  fclose(img);
  printf("Image file created successfully.\n");
  return 0;
}
