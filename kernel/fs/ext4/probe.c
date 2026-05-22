#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/ext4.h"
#include "kernel/log.h"
#include "kernel/types.h"

// EXT4 on-disk fields are little-endian. Read byte-by-byte to avoid relying on
// host endianness or aligned integer loads.
static uint16 ext4_read_le16(const uint8 *p)
{
	return (uint16) p[0] | ((uint16) p[1] << 8);
}

static uint32 ext4_read_le32(const uint8 *p)
{
	return (uint32) p[0] | ((uint32) p[1] << 8) | ((uint32) p[2] << 16) |
	       ((uint32) p[3] << 24);
}

// Some 64-bit EXT4 fields are stored as separate low/high 32-bit values for
// compatibility with older descriptor layouts.
static uint64 ext4_read_le64_split(const uint8 *lo, const uint8 *hi)
{
	return ((uint64) ext4_read_le32(hi) << 32) | ext4_read_le32(lo);
}

// This reader is intentionally read-only and small. Unknown incompatible
// features are rejected before later code interprets unsupported disk layouts.
static int ext4_check_features(struct ext4_fs *fs)
{
	uint32 supported_incompat =
	    EXT4_FEATURE_INCOMPAT_FILETYPE | EXT4_FEATURE_INCOMPAT_EXTENTS |
	    EXT4_FEATURE_INCOMPAT_64BIT | EXT4_FEATURE_INCOMPAT_FLEX_BG;
	uint32 supported_ro_compat = EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER |
				     EXT4_FEATURE_RO_COMPAT_LARGE_FILE |
				     EXT4_FEATURE_RO_COMPAT_HUGE_FILE |
				     EXT4_FEATURE_RO_COMPAT_DIR_NLINK |
				     EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE |
				     EXT4_FEATURE_RO_COMPAT_METADATA_CSUM;
	uint32 unknown_incompat = fs->feature_incompat & ~supported_incompat;
	uint32 unknown_ro_compat = fs->feature_ro_compat & ~supported_ro_compat;

	if (unknown_incompat != 0) {
		LOG_ERROR("ext4: unsupported incompat features: %x",
			  unknown_incompat);
		return -1;
	}

	if (unknown_ro_compat != 0) {
		LOG_ERROR("ext4: unsupported ro_compat features: %x",
			  unknown_ro_compat);
		return -1;
	}

	if ((fs->feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS) == 0) {
		LOG_ERROR("ext4: non-extent inode data is not supported");
		return -1;
	}

	return 0;
}

// Read just enough of the superblock to drive the minimal read-only path:
// block sizing, inode table location parameters, and feature gates.
int ext4_mount_min(uint32 dev, struct ext4_fs *fs)
{
	struct buf *b = bread(dev, 0);
	const uint8 *sb = b->data + EXT4_SUPER_OFFSET;
	uint16 magic = ext4_read_le16(sb + EXT4_SB_MAGIC);

	if (magic != EXT4_SUPER_MAGIC) {
		LOG_TRACE("ext4: dev %d has no ext4 superblock", dev);
		brelse(b);
		return -1;
	}

	uint32 log_block_size = ext4_read_le32(sb + EXT4_SB_LOG_BLOCK_SIZE);
	uint32 blocks_lo = ext4_read_le32(sb + EXT4_SB_BLOCKS_COUNT_LO);
	uint32 blocks_hi = ext4_read_le32(sb + EXT4_SB_BLOCKS_COUNT_HI);

	fs->dev = dev;
	fs->block_size = 1024U << log_block_size;
	fs->blocks_count = ((uint64) blocks_hi << 32) | blocks_lo;
	fs->inodes_count = ext4_read_le32(sb + EXT4_SB_INODES_COUNT);
	fs->blocks_per_group = ext4_read_le32(sb + EXT4_SB_BLOCKS_PER_GROUP);
	fs->inodes_per_group = ext4_read_le32(sb + EXT4_SB_INODES_PER_GROUP);
	fs->inode_size = ext4_read_le16(sb + EXT4_SB_INODE_SIZE);
	fs->desc_size = ext4_read_le16(sb + EXT4_SB_DESC_SIZE);
	// For 1 KiB block filesystems, block 0 is before the superblock and the
	// GDT starts at block 2. For larger block sizes, the GDT starts at
	// block 1.
	fs->group_desc_block = fs->block_size == 1024 ? 2 : 1;
	fs->feature_compat = ext4_read_le32(sb + EXT4_SB_FEATURE_COMPAT);
	fs->feature_incompat = ext4_read_le32(sb + EXT4_SB_FEATURE_INCOMPAT);
	fs->feature_ro_compat = ext4_read_le32(sb + EXT4_SB_FEATURE_RO_COMPAT);

	brelse(b);
	return ext4_check_features(fs);
}

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
int ext4_read_inode_min(struct ext4_fs *fs, uint32 ino,
			struct ext4_inode_min *inode)
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
int ext4_read_extent_header(const struct ext4_inode_min *inode,
			    struct ext4_extent_header_min *header)
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
int ext4_read_extent_at(const struct ext4_inode_min *inode, uint16 index,
			struct ext4_extent_min *extent)
{
	// In extent mode, inode.i_block starts with a 12-byte extent header.
	// Leaf extent entries are packed immediately after that header.
	const uint8 *raw_extent =
	    inode->block + EXT4_EXT_HEADER_SIZE + (index * EXT4_EXTENT_SIZE);

	extent->block = ext4_read_le32(raw_extent + EXT4_EXTENT_BLOCK);
	extent->len = ext4_read_le16(raw_extent + EXT4_EXTENT_LEN);
	extent->start =
	    ((uint64) ext4_read_le16(raw_extent + EXT4_EXTENT_START_HI)
	     << 32) |
	    ext4_read_le32(raw_extent + EXT4_EXTENT_START_LO);

	return 0;
}

// Parse one physical directory data block as ext4_dir_entry_2 records and
// print names for bring-up. Directory entry names are not NUL-terminated.
int ext4_probe_dir_block(struct ext4_fs *fs, uint64 block)
{
	uint64 byte_offset = block * fs->block_size;
	uint64 cache_block = byte_offset / BSIZE;
	uint64 offset_in_cache = byte_offset % BSIZE;
	struct buf *b = bread(fs->dev, cache_block);
	uint64 offset = 0;

	while (offset < fs->block_size) {
		const uint8 *dirent = b->data + offset_in_cache + offset;
		uint32 ino = ext4_read_le32(dirent + EXT4_DIRENT_INODE);
		uint16 rec_len = ext4_read_le16(dirent + EXT4_DIRENT_REC_LEN);
		uint8 name_len = *(dirent + EXT4_DIRENT_NAME_LEN);
		uint8 file_type = *(dirent + EXT4_DIRENT_FILE_TYPE);

		// rec_len must at least cover the fixed dirent header, and the
		// record must stay inside the current filesystem block. Without
		// this guard, malformed data can make offset stall or overrun.
		if (rec_len < EXT4_DIRENT_NAME ||
		    offset + rec_len > fs->block_size) {
			LOG_ERROR("ext4: invalid dirent rec_len=%d at offset=%d",
				  rec_len, offset);
			brelse(b);
			return -1;
		}

		if (ino != 0 && name_len <= EXT4_NAME_MAX &&
		    EXT4_DIRENT_NAME + name_len <= rec_len) {
			char name[EXT4_NAME_MAX + 1];
			memcpy(name, dirent + EXT4_DIRENT_NAME, name_len);
			name[name_len] = 0;
			LOG_INFO("ext4: dirent inode=%d type=%d name=%s", ino,
				 file_type, name);
		}

		offset += rec_len;
	}

	brelse(b);
	return 0;
}

// Temporary boot-time probe used while bringing up the EXT4 reader.
int ext4_probe(uint32 dev)
{
	struct ext4_fs fs;

	if (ext4_mount_min(dev, &fs) < 0) {
		LOG_ERROR("ext4: probe failed");
		return -1;
	}

	LOG_INFO("ext4: dev %d detected", dev);
	LOG_INFO("ext4: block_size=%d blocks=%d inodes=%d", fs.block_size,
		 fs.blocks_count, fs.inodes_count);
	LOG_INFO("ext4: inode_size=%d desc_size=%d", fs.inode_size,
		 fs.desc_size);
	uint64 inode_table_block;
	ext4_read_inode_table_block(&fs, 0, &inode_table_block);
	LOG_INFO("ext4: group 0 inode_table=%d", inode_table_block);

	struct ext4_inode_min root;
	if (ext4_read_inode_min(&fs, EXT4_ROOT_INO, &root) == 0) {
		LOG_INFO("ext4: root inode mode=%x size=%d blocks=%d flags=%x",
			 root.mode, root.size, root.blocks_lo, root.flags);
		struct ext4_extent_header_min eh;
		if (ext4_read_extent_header(&root, &eh) == 0) {
			LOG_INFO("ext4: root extent magic=%x entries=%d max=%d depth=%d",
				 eh.magic, eh.entries, eh.max, eh.depth);
			if (eh.depth == 0 && eh.entries > 0) {
				struct ext4_extent_min ex;
				ext4_read_extent_at(&root, 0, &ex);
				LOG_INFO("ext4: root extent[0] block=%d len=%d start=%d",
					 ex.block, ex.len, ex.start);
				ext4_probe_dir_block(&fs, ex.start);
			}
		}
	}

	LOG_INFO("ext4: features compat=%x incompat=%x ro_compat=%x",
		 fs.feature_compat, fs.feature_incompat, fs.feature_ro_compat);

	return 0;
}
