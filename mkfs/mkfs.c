#include "kernel/fs.h"
#include "kernel/types.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define BLOCK_SIZE 4096
#define MAGIC_NUM 0x0B8EE2E0
#define TOTAL_BLOCKS 10000
#define VFS_DIR 0x0001
#define VFS_FILE 0x0010

int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf("Usage: mkfs <image file> <init_bin>\n");
		return -1;
	}

	// open the image file for writing in binary mode
	FILE *img = fopen(argv[1], "wb");
	if (!img) {
		perror("Failed to open image file");
		return -1;
	}

	// Open the compiled user-space binary
	FILE *init_bin = fopen(argv[2], "rb");
	if (!init_bin) {
		perror("Failed to open init_bin file");
		fclose(img);
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
	// Block 4 to 10: Inode Area (e.g., 7 blocks * 64 inodes/block = 448
	// inodes) Block 11 to 9999: Data Area
	super_block.ibitmap_area_start = 2;
	super_block.dbitmap_area_start = 3;
	super_block.inode_area_start = 4;
	super_block.data_area_start = 11;

	fseek(img, 1 * BLOCK_SIZE, SEEK_SET);
	fwrite(&super_block, sizeof(super_block), 1, img);

	// Get init_bin file size
	fseek(init_bin, 0, SEEK_END);
	long init_size = ftell(init_bin);
	rewind(init_bin);

	// Calculate how many data blocks the binary needs
	uint32 init_blocks_needed = (init_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	// Ensure it fits within direct blocks limit (12 blocks)
	assert(init_blocks_needed <= 10 &&
	       "Init binary is too large for direct blocks");

	// Setup Root Inode (Inode #0)
	struct disk_inode root_inode = {0};
	root_inode.type = VFS_DIR;
	root_inode.nlinks = 2; // . and .. pointer to self
	// The root directory now contains 3 entries: ".", "..", and "init"
	root_inode.size = 3 * sizeof(struct disk_dir_entry);
	root_inode.blocks[0] =
	    super_block.data_area_start; // Data block for Root

	// Setup Init Binary Inode (Inode #1)
	struct disk_inode init_inode = {0};
	init_inode.type = VFS_FILE;
	init_inode.nlinks = 1;
	init_inode.size = init_size;

	// Allocate data blocks for init binary sequentially after the root
	// block
	for (uint32 i = 0; i < init_blocks_needed; i++) {
		init_inode.blocks[i] = super_block.data_area_start + 1 + i;
	}

	// Write Root Inode to the beginning of the Inode Area
	fseek(img, super_block.inode_area_start * BLOCK_SIZE, SEEK_SET);
	fwrite(&root_inode, sizeof(struct disk_inode), 1, img);

	// Write Init Inode right after the Root Inode (offset by 64 bytes)
	fseek(img,
	      super_block.inode_area_start * BLOCK_SIZE +
		  sizeof(struct disk_inode),
	      SEEK_SET);
	fwrite(&init_inode, sizeof(struct disk_inode), 1, img);

	// Write directory entries to the root data block
	struct disk_dir_entry root_entries[3] = {0};
	root_entries[0].inode_num = 0; // .
	strcpy(root_entries[0].name, ".");

	root_entries[1].inode_num = 0; // ..
	strcpy(root_entries[1].name, "..");

	root_entries[2].inode_num = 1; // init_bin
	strcpy(root_entries[2].name,
	       "init"); // The name you will use to lookup in the OS

	fseek(img, root_inode.blocks[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(root_entries, sizeof(struct disk_dir_entry), 3, img);

	// Write the actual init_bin data into its allocated blocks
	char rw_buffer[BLOCK_SIZE];
	for (uint32 i = 0; i < init_blocks_needed; i++) {
		memset(rw_buffer, 0, BLOCK_SIZE);
		size_t bytes_read = fread(rw_buffer, 1, BLOCK_SIZE, init_bin);
		if (bytes_read > 0) {
			fseek(img, init_inode.blocks[i] * BLOCK_SIZE, SEEK_SET);
			fwrite(rw_buffer, 1, BLOCK_SIZE, img);
		}
	}

	// Update Inode Bitmap (Inode 0 and 1 are used -> 00000011 in binary ->
	// 0x03)
	uint8_t ibitmap_block[BLOCK_SIZE] = {0};
	ibitmap_block[0] = 0x03;
	fseek(img, super_block.ibitmap_area_start * BLOCK_SIZE, SEEK_SET);
	fwrite(ibitmap_block, BLOCK_SIZE, 1, img);

	// Update Data Bitmap (Root block + init_blocks_needed are used)
	// For example, if init needs 2 blocks, total used is 3 blocks ->
	// 00000111 -> 0x07
	uint8_t dbitmap_block[BLOCK_SIZE] = {0};
	uint8_t data_used_mask = (1 << (1 + init_blocks_needed)) - 1;
	dbitmap_block[0] = data_used_mask;
	fseek(img, super_block.dbitmap_area_start * BLOCK_SIZE, SEEK_SET);
	fwrite(dbitmap_block, BLOCK_SIZE, 1, img);

	// Clean up
	fclose(init_bin);
	fclose(img);
	printf("Image file created successfully, init binary injected.\n");
	return 0;
}
