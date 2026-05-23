#ifndef __KERNEL_EXT4_H__
#define __KERNEL_EXT4_H__

#include "kernel/types.h"

/*
 * Minimal read-only EXT4 definitions.
 *
 * Field offsets follow the Linux kernel EXT4 superblock documentation:
 * https://docs.kernel.org/filesystems/ext4/super.html
 */

#define EXT4_SUPER_OFFSET 1024
#define EXT4_SUPER_MAGIC 0xEF53
#define EXT4_ROOT_INO 2

/* Superblock field offsets, relative to EXT4_SUPER_OFFSET. */
#define EXT4_SB_INODES_COUNT 0x00
#define EXT4_SB_BLOCKS_COUNT_LO 0x04
#define EXT4_SB_FIRST_DATA_BLOCK 0x14
#define EXT4_SB_LOG_BLOCK_SIZE 0x18
#define EXT4_SB_BLOCKS_PER_GROUP 0x20
#define EXT4_SB_INODES_PER_GROUP 0x28
#define EXT4_SB_MAGIC 0x38
#define EXT4_SB_INODE_SIZE 0x58
#define EXT4_SB_FEATURE_COMPAT 0x5c
#define EXT4_SB_FEATURE_INCOMPAT 0x60
#define EXT4_SB_FEATURE_RO_COMPAT 0x64
#define EXT4_SB_DESC_SIZE 0xfe
#define EXT4_SB_BLOCKS_COUNT_HI 0x150

/*
 * Group descriptor field offsets.
 * With 64bit enabled, high 32-bit fields are present after the low fields.
 */
#define EXT4_GD_INODE_TABLE_LO 0x08
#define EXT4_GD_INODE_TABLE_HI 0x28

/* Inode field offsets, relative to the start of struct ext4_inode. */
#define EXT4_INODE_MODE 0x00
#define EXT4_INODE_SIZE_LO 0x04
#define EXT4_INODE_BLOCKS_LO 0x1c
#define EXT4_INODE_FLAGS 0x20
#define EXT4_INODE_BLOCK 0x28
#define EXT4_INODE_SIZE_HI 0x6c
#define EXT4_INODE_BLOCK_BYTES 60

#define EXT4_EXT_MAGIC 0xF30A

/* Extent header offsets, relative to inode.i_block. */
#define EXT4_EXT_HEADER_MAGIC 0x00
#define EXT4_EXT_HEADER_ENTRIES 0x02
#define EXT4_EXT_HEADER_MAX 0x04
#define EXT4_EXT_HEADER_DEPTH 0x06
#define EXT4_EXT_HEADER_GENERATION 0x08
#define EXT4_EXT_HEADER_SIZE 12

/* Extent leaf entry offsets, relative to the start of struct ext4_extent. */
#define EXT4_EXTENT_BLOCK 0x00
#define EXT4_EXTENT_LEN 0x04
#define EXT4_EXTENT_START_HI 0x06
#define EXT4_EXTENT_START_LO 0x08
#define EXT4_EXTENT_SIZE 12

/* ext4_dir_entry_2 offsets, relative to one directory entry record. */
#define EXT4_DIRENT_INODE 0x00
#define EXT4_DIRENT_REC_LEN 0x04
#define EXT4_DIRENT_NAME_LEN 0x06
#define EXT4_DIRENT_FILE_TYPE 0x07
#define EXT4_DIRENT_NAME 0x08
#define EXT4_NAME_MAX 255

/* ext4_dir_entry_2 file_type values. */
#define EXT4_FT_REG_FILE 1
#define EXT4_FT_DIR 2

/* Feature set: compatible features may be ignored by old implementations. */
#define EXT4_FEATURE_COMPAT_DIR_PREALLOC 0x0001
#define EXT4_FEATURE_COMPAT_IMAGIC_INODES 0x0002
#define EXT4_FEATURE_COMPAT_HAS_JOURNAL 0x0004
#define EXT4_FEATURE_COMPAT_EXT_ATTR 0x0008
#define EXT4_FEATURE_COMPAT_RESIZE_INODE 0x0010
#define EXT4_FEATURE_COMPAT_DIR_INDEX 0x0020

/* Feature set: unsupported incompatible features must reject mounting. */
#define EXT4_FEATURE_INCOMPAT_FILETYPE 0x0002
#define EXT4_FEATURE_INCOMPAT_EXTENTS 0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT 0x0080
#define EXT4_FEATURE_INCOMPAT_FLEX_BG 0x0200

/* Feature set: unsupported read-only-compatible features reject writes. */
#define EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001
#define EXT4_FEATURE_RO_COMPAT_LARGE_FILE 0x0002
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE 0x0008
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK 0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE 0x0040
#define EXT4_FEATURE_RO_COMPAT_METADATA_CSUM 0x0400

struct ext4_fs {
	/* Kernel virtio block device number that contains this EXT4 image. */
	uint32 dev;
	/* EXT4 filesystem block size in bytes, decoded from s_log_block_size. */
	uint32 block_size;
	/* Total filesystem blocks, not bytes. Uses low+high superblock fields. */
	uint64 blocks_count;
	/* Total inode count in the filesystem. */
	uint32 inodes_count;
	/* Number of filesystem blocks in each block group. */
	uint32 blocks_per_group;
	/* Number of inodes in each block group. Used to locate an inode. */
	uint32 inodes_per_group;
	/* On-disk inode size in bytes. This image uses 256-byte inodes. */
	uint16 inode_size;
	/* On-disk group descriptor size in bytes. 64bit images use larger descs. */
	uint16 desc_size;
	/* Filesystem block number where the group descriptor table starts. */
	uint64 group_desc_block;
	/* Compatible feature bits from the superblock. */
	uint32 feature_compat;
	/* Incompatible feature bits; unknown bits must reject the image. */
	uint32 feature_incompat;
	/* Read-only compatible feature bits; unknown bits reject this reader. */
	uint32 feature_ro_compat;
};

/* Minimal inode snapshot used during reader bring-up. */
struct ext4_inode_min {
	/* i_mode: file type bits plus permission bits, e.g. 0x41ed for a dir. */
	uint16 mode;
	/* i_size: file length in bytes, assembled from i_size_lo/i_size_high. */
	uint64 size;
	/* i_blocks_lo: number of 512-byte sectors reserved for this inode. */
	uint32 blocks_lo;
	/* i_flags: inode flags. EXT4_EXTENTS_FL means i_block stores extents. */
	uint32 flags;
	/*
	 * Copy of the on-disk inode.i_block[60] byte array.
	 *
	 * This is not a filesystem block number. Its meaning depends on inode
	 * format:
	 * - with EXT4_EXTENTS_FL: starts with ext4_extent_header, then extent
	 *   entries or index entries;
	 * - without EXT4_EXTENTS_FL: contains direct/indirect block pointers.
	 */
	uint8 block[EXT4_INODE_BLOCK_BYTES];
};

/* Header stored at the start of inode.i_block when EXT4_EXTENTS_FL is set. */
struct ext4_extent_header_min {
	/* eh_magic: must be 0xf30a for an EXT4 extent tree node. */
	uint16 magic;
	/* eh_entries: number of valid entries following this header. */
	uint16 entries;
	/* eh_max: maximum entry capacity of this node, not valid entry count. */
	uint16 max;
	/* eh_depth: 0 means leaf extents; >0 means index nodes. */
	uint16 depth;
	/* eh_generation: tree generation, unused by the current read-only path. */
	uint32 generation;
};

/* One depth-0 extent entry mapping logical file blocks to physical blocks. */
struct ext4_extent_min {
	/*
	 * ee_block: first logical block inside the file covered by this extent.
	 * This is not a physical disk block number.
	 */
	uint32 block;
	/* ee_len: number of consecutive filesystem blocks in this extent. */
	uint16 len;
	/*
	 * ee_start_hi/ee_start_lo combined: first physical filesystem block on
	 * disk. This is the block number passed to bread() after conversion to
	 * the kernel buffer-cache block size.
	 */
	uint64 start;
};

int ext4_probe(uint32 dev);
int ext4_mount_min(uint32 dev, struct ext4_fs *fs);
int ext4_read_inode_table_block(struct ext4_fs *fs, uint32 group,
				uint64 *block);
int ext4_read_inode_min(struct ext4_fs *fs, uint32 ino,
			struct ext4_inode_min *inode);
int ext4_read_extent_header(const struct ext4_inode_min *inode,
			    struct ext4_extent_header_min *header);
int ext4_read_extent_at(const struct ext4_inode_min *inode, uint16 index,
			struct ext4_extent_min *extent);
int ext4_probe_dir_block(struct ext4_fs *fs, uint64 block);
int ext4_probe_dir_inode(struct ext4_fs *fs, struct ext4_inode_min *dir);
int ext4_lookup_in_dir(struct ext4_fs *fs, struct ext4_inode_min *dir,
		       const char *name, uint32 *ino, uint8 *file_type);
int ext4_read_file_at(struct ext4_fs *fs, struct ext4_inode_min *inode,
		      uint64 offset, uint8 *dst, uint64 len);
int ext4_lookup_path(struct ext4_fs *fs, const char *path,
		     struct ext4_inode_min *inode, uint8 *file_type);

#endif
