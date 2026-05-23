#include "kernel/bcache.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "ext4.h"
#include "helper.h"

static struct ext4_fs ext4_root_fs;
static int ext4_root_mounted;

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
int ext4_mount(uint32 dev, struct ext4_fs *fs)
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

// Mount the boot root EXT4 image and keep its decoded superblock state alive.
//
// The probe path can use a stack ext4_fs, but VFS lookup/read code needs this
// state to outlive the caller that performed the mount.
int ext4_mount_root(uint32 dev)
{
	if (ext4_mount(dev, &ext4_root_fs) < 0) {
		ext4_root_mounted = 0;
		return -1;
	}

	ext4_root_mounted = 1;
	return 0;
}

// Return the mounted root EXT4 context, or 0 before ext4_mount_root succeeds.
//
// This lets future VFS code fail cleanly instead of reading through an
// uninitialized mount context.
struct ext4_fs *ext4_get_root_fs(void)
{
	if (!ext4_root_mounted) {
		return 0;
	}

	return &ext4_root_fs;
}
