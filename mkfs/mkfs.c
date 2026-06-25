#include "kernel/types.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BLOCK_SIZE 4096
#define MAGIC_NUM 0x0B8EE2E0
#define TOTAL_BLOCKS 4096
#define VFS_DIR 0x0001
#define VFS_FILE 0x0010
#define MAX_FILES 64
#define MAX_DIRECT_BLOCKS 10

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
//
// Planned indirect-block layout without changing the on-disk inode size:
// - blocks[0..9]:  direct data blocks
// - blocks[10]:    single-indirect block; stores uint32 data block numbers
// - blocks[11]:    double-indirect block; stores uint32 single-indirect block
//                  numbers, and those second-level blocks store data block
//                  numbers.
struct disk_inode {
	uint16 type;	   // File or directory
	uint16 nlinks;	   // Number of hard links
	uint32 size;	   // Size in bytes
	uint32 blocks[12]; // Direct/indirect block address slots
	uint32 padding[2]; // align to 64
};

// A simple directory entry structure
struct disk_dir_entry {
	uint32 inode_num; // Inode number
	char name[28];	  // File/Directory name
};

struct input_file {
	char host_path[256];
	char fs_name[28];
	FILE *fp;
	long size;
	uint32 blocks_needed;
	struct disk_inode inode;
};

static void set_bitmap(uint8_t *bitmap, uint32 bit)
{
	bitmap[bit / 8] |= 1 << (bit % 8);
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		printf("Usage: mkfs <image file> <host_path:fs_name>...\n");
		return -1;
	}

	int file_count = argc - 2;
	if (file_count > MAX_FILES) {
		printf("Too many input files: %d, max %d\n", file_count,
		       MAX_FILES);
		return -1;
	}

	struct input_file files[MAX_FILES] = {0};
	for (int i = 0; i < file_count; i++) {
		char *spec = argv[i + 2];
		char *colon = strchr(spec, ':');
		if (!colon || colon == spec || colon[1] == '\0') {
			printf("Invalid file spec: %s\n", spec);
			printf("Expected <host_path:fs_name>\n");
			return -1;
		}

		uint32 path_len = colon - spec;
		if (path_len == 0 || path_len >= sizeof(files[i].host_path)) {
			printf("Invalid host path in spec: %s\n", spec);
			return -1;
		}
		if ((uint32) strlen(colon + 1) >= sizeof(files[i].fs_name)) {
			printf("Filesystem name too long: %s\n", colon + 1);
			return -1;
		}

		memcpy(files[i].host_path, spec, path_len);
		files[i].host_path[path_len] = '\0';
		strcpy(files[i].fs_name, colon + 1);
	}

	// open the image file for writing in binary mode
	FILE *img = fopen(argv[1], "wb");
	if (!img) {
		perror("Failed to open image file");
		return -1;
	}

	for (int i = 0; i < file_count; i++) {
		files[i].fp = fopen(files[i].host_path, "rb");
		if (!files[i].fp) {
			perror("Failed to open input file");
			fclose(img);
			return -1;
		}

		fseek(files[i].fp, 0, SEEK_END);
		files[i].size = ftell(files[i].fp);
		rewind(files[i].fp);

		files[i].blocks_needed =
		    (files[i].size + BLOCK_SIZE - 1) / BLOCK_SIZE;
		assert(files[i].blocks_needed <= MAX_DIRECT_BLOCKS &&
		       "input file is too large for direct blocks");
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
	// inodes)
	// Block 11 to 9999: Data Area
	super_block.ibitmap_area_start = 2;
	super_block.dbitmap_area_start = 3;
	super_block.inode_area_start = 4;
	super_block.data_area_start = 11;

	fseek(img, 1 * BLOCK_SIZE, SEEK_SET);
	fwrite(&super_block, sizeof(super_block), 1, img);

	// Setup Root Inode (Inode #0)
	struct disk_inode root_inode = {0};
	root_inode.type = VFS_DIR;
	root_inode.nlinks = 2; // . and .. pointer to self
	root_inode.size = (file_count + 2) * sizeof(struct disk_dir_entry);
	root_inode.blocks[0] =
	    super_block.data_area_start; // Data block for Root

	uint32 next_data_block = super_block.data_area_start + 1;
	for (int i = 0; i < file_count; i++) {
		files[i].inode.type = VFS_FILE;
		files[i].inode.nlinks = 1;
		files[i].inode.size = files[i].size;

		for (uint32 j = 0; j < files[i].blocks_needed; j++) {
			files[i].inode.blocks[j] = next_data_block++;
		}
	}

	// Write Root Inode to the beginning of the Inode Area
	fseek(img,
	      (super_block.inode_area_start * BLOCK_SIZE) +
		  (1 * sizeof(struct disk_inode)),
	      SEEK_SET);
	fwrite(&root_inode, sizeof(struct disk_inode), 1, img);

	for (int i = 0; i < file_count; i++) {
		fseek(img,
		      super_block.inode_area_start * BLOCK_SIZE +
			  (i + 2) * sizeof(struct disk_inode),
		      SEEK_SET);
		fwrite(&files[i].inode, sizeof(struct disk_inode), 1, img);
	}

	// Write directory entries to the root data block
	struct disk_dir_entry root_entries[MAX_FILES + 2] = {0};
	root_entries[0].inode_num = 1; // .
	strcpy(root_entries[0].name, ".");

	root_entries[1].inode_num = 1; // ..
	strcpy(root_entries[1].name, "..");

	for (int i = 0; i < file_count; i++) {
		root_entries[i + 2].inode_num = i + 2;
		strcpy(root_entries[i + 2].name, files[i].fs_name);
	}

	fseek(img, root_inode.blocks[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(root_entries, sizeof(struct disk_dir_entry), file_count + 2,
	       img);

	// Write the actual file data into its allocated blocks
	char rw_buffer[BLOCK_SIZE];
	for (int i = 0; i < file_count; i++) {
		for (uint32 j = 0; j < files[i].blocks_needed; j++) {
			memset(rw_buffer, 0, BLOCK_SIZE);
			size_t bytes_read =
			    fread(rw_buffer, 1, BLOCK_SIZE, files[i].fp);
			if (bytes_read > 0) {
				fseek(img,
				      files[i].inode.blocks[j] * BLOCK_SIZE,
				      SEEK_SET);
				fwrite(rw_buffer, 1, BLOCK_SIZE, img);
			}
		}
	}

	// Update Inode Bitmap
	uint8_t ibitmap_block[BLOCK_SIZE] = {0};
	for (uint32 i = 0; i < (uint32) file_count + 2; i++) {
		set_bitmap(ibitmap_block, i);
	}
	fseek(img, super_block.ibitmap_area_start * BLOCK_SIZE, SEEK_SET);
	fwrite(ibitmap_block, BLOCK_SIZE, 1, img);

	// Update Data Bitmap
	uint8_t dbitmap_block[BLOCK_SIZE] = {0};
	set_bitmap(dbitmap_block, 0);
	for (int i = 0; i < file_count; i++) {
		for (uint32 j = 0; j < files[i].blocks_needed; j++) {
			set_bitmap(dbitmap_block,
				   files[i].inode.blocks[j] -
				       super_block.data_area_start);
		}
	}
	fseek(img, super_block.dbitmap_area_start * BLOCK_SIZE, SEEK_SET);
	fwrite(dbitmap_block, BLOCK_SIZE, 1, img);

	// Clean up
	for (int i = 0; i < file_count; i++) {
		fclose(files[i].fp);
	}
	fclose(img);
	printf("Image file created successfully, %d files injected.\n",
	       file_count);
	return 0;
}
