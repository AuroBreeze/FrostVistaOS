#include "kernel/fs.h"
#include "kernel/types.h"
#include "kernel/bcache.h"
#include "kernel/log.h"
#include "ext4.h"
#include "helper.h"

// Read bg_inode_table for one block group. EXT4 block numbers are converted to
// byte offsets first because the buffer cache block size is a kernel policy,
// not necessarily the same as the filesystem block size.
int ext4_read_inode_table_block(struct ext4_fs *fs, uint32 group, uint64 *block)
{
	uint64 desc_offset = (uint64) group * fs->desc_size;
	uint64 byte_offset =
	    (fs->group_desc_block * fs->block_size) + desc_offset;
	uint64 cache_block = byte_offset / BSIZE;
	uint64 offset_in_block = byte_offset % BSIZE;
	struct buf *b = bread(fs->dev, cache_block);
	const uint8 *gd = b->data + offset_in_block;

	*block = ext4_read_le64_split(gd + EXT4_GD_INODE_TABLE_LO,
				      gd + EXT4_GD_INODE_TABLE_HI);

	brelse(b);
	return 0;
}

// Locate one inode by its global inode number and read only the fields needed
// for the next probe step. EXT4 inode numbers start at 1, so group/index math
// uses ino - 1.
int ext4_read_inode(struct ext4_fs *fs, uint32 ino, struct ext4_inode *inode)
{
	uint32 group = (ino - 1) / fs->inodes_per_group;
	uint32 index = (ino - 1) % fs->inodes_per_group;
	uint64 inode_table_block;

	if (ext4_read_inode_table_block(fs, group, &inode_table_block) < 0) {
		return -1;
	}

	uint64 inode_byte_offset = (inode_table_block * fs->block_size) +
				   ((uint64) index * fs->inode_size);
	uint64 cache_block = inode_byte_offset / BSIZE;
	uint64 offset_in_block = inode_byte_offset % BSIZE;
	struct buf *b = bread(fs->dev, cache_block);
	const uint8 *raw_inode = b->data + offset_in_block;

	inode->mode = ext4_read_le16(raw_inode + EXT4_INODE_MODE);
	inode->size =
	    ((uint64) ext4_read_le32(raw_inode + EXT4_INODE_SIZE_HI) << 32) |
	    ext4_read_le32(raw_inode + EXT4_INODE_SIZE_LO);
	inode->blocks_lo = ext4_read_le32(raw_inode + EXT4_INODE_BLOCKS_LO);
	inode->flags = ext4_read_le32(raw_inode + EXT4_INODE_FLAGS);
	memcpy(inode->block, raw_inode + EXT4_INODE_BLOCK,
	       EXT4_INODE_BLOCK_BYTES);

	brelse(b);
	return 0;
}

// Interpret inode.i_block as an extent tree header. Callers should only use
// this for inodes with EXT4_EXTENTS_FL set.
int ext4_read_extent_header(const struct ext4_inode *inode,
			    struct ext4_extent_header *header)
{
	const uint8 *raw_header = inode->block;

	header->magic = ext4_read_le16(raw_header + EXT4_EXT_HEADER_MAGIC);
	header->entries = ext4_read_le16(raw_header + EXT4_EXT_HEADER_ENTRIES);
	header->max = ext4_read_le16(raw_header + EXT4_EXT_HEADER_MAX);
	header->depth = ext4_read_le16(raw_header + EXT4_EXT_HEADER_DEPTH);
	header->generation =
	    ext4_read_le32(raw_header + EXT4_EXT_HEADER_GENERATION);

	if (header->magic != EXT4_EXT_MAGIC) {
		LOG_ERROR("ext4: invalid extent header magic: %x",
			  header->magic);
		return -1;
	}

	return 0;
}

// Read one leaf extent entry from inode.i_block. This only handles depth=0
// callers; index nodes will be added later if needed.
int ext4_read_extent(const struct ext4_inode *inode, uint16 index,
		     struct ext4_extent *extent)
{
	// In extent mode, inode.i_block starts with a 12-byte extent header.
	// Leaf extent entries are packed immediately after that header.
	const uint8 *raw_extent = inode->block + EXT4_EXT_HEADER_SIZE +
				  ((uint64) index * EXT4_EXTENT_SIZE);

	extent->block = ext4_read_le32(raw_extent + EXT4_EXTENT_BLOCK);
	extent->len = ext4_read_le16(raw_extent + EXT4_EXTENT_LEN);
	extent->start =
	    ((uint64) ext4_read_le16(raw_extent + EXT4_EXTENT_START_HI) << 32) |
	    ext4_read_le32(raw_extent + EXT4_EXTENT_START_LO);

	return 0;
}

static int ext4_find_extent(const struct ext4_inode *inode,
			    uint32 logical_block, struct ext4_extent *extent)
{
	struct ext4_extent_header eh;

	if (ext4_read_extent_header(inode, &eh) < 0) {
		return -1;
	}

	if (eh.depth != 0) {
		LOG_ERROR("ext4: indexed file extents are not supported yet");
		return -1;
	}

	for (uint16 i = 0; i < eh.entries; i++) {
		struct ext4_extent ex;
		ext4_read_extent(inode, i, &ex);
		if (logical_block >= ex.block &&
		    logical_block < ex.block + ex.len) {
			*extent = ex;
			return 0;
		}
	}

	return -1;
}

int ext4_read_file(struct ext4_fs *fs, struct ext4_inode *inode, uint64 offset,
		   uint8 *dst, uint64 len)
{
	uint64 done = 0;

	if (offset >= inode->size) {
		return 0;
	}

	if (offset + len > inode->size) {
		len = inode->size - offset;
	}

	while (done < len) {
		uint64 file_offset = offset + done;
		uint32 logical_block = file_offset / fs->block_size;
		uint32 offset_in_fs_block = file_offset % fs->block_size;
		struct ext4_extent ex;

		if (ext4_find_extent(inode, logical_block, &ex) < 0) {
			return -1;
		}

		uint64 physical_block = ex.start + (logical_block - ex.block);
		uint64 byte_offset =
		    (physical_block * fs->block_size) + offset_in_fs_block;
		uint64 cache_block = byte_offset / BSIZE;
		uint64 offset_in_cache = byte_offset % BSIZE;
		uint64 chunk = len - done;
		uint64 cache_left = BSIZE - offset_in_cache;
		uint64 fs_block_left = fs->block_size - offset_in_fs_block;

		if (chunk > cache_left) {
			chunk = cache_left;
		}
		if (chunk > fs_block_left) {
			chunk = fs_block_left;
		}

		struct buf *b = bread(fs->dev, cache_block);
		memcpy(dst + done, b->data + offset_in_cache, chunk);
		brelse(b);

		done += chunk;
	}

	return done;
}
