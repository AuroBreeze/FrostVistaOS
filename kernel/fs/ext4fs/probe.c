#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "ext4.h"
#include "helper.h"

static void ext4_dump_bytes(const char *label, const uint8 *buf, uint32 len)
{
	for (uint32 i = 0; i < len; i += 16) {
		LOG_INFO("ext4: %s +%d: %x %x %x %x %x %x %x %x %x %x %x %x %x "
			 "%x %x %x ",
			 label, i, buf[i + 0], buf[i + 1], buf[i + 2],
			 buf[i + 3], buf[i + 4], buf[i + 5], buf[i + 6],
			 buf[i + 7], buf[i + 8], buf[i + 9], buf[i + 10],
			 buf[i + 11], buf[i + 12], buf[i + 13], buf[i + 14],
			 buf[i + 15]);
	}
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
			LOG_ERROR(
			    "ext4: invalid dirent rec_len=%d at offset=%d",
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

// Probe a directory inode by following its first-level extents and printing the
// directory entries in each referenced data block. This currently supports only
// depth-0 extent trees.
int ext4_probe_dir_inode(struct ext4_fs *fs, struct ext4_inode *dir)
{
	struct ext4_extent_header eh;

	if (ext4_read_extent_header(dir, &eh) < 0) {
		return -1;
	}

	LOG_INFO("ext4: dir extent magic=%x entries=%d max=%d depth=%d",
		 eh.magic, eh.entries, eh.max, eh.depth);

	if (eh.depth != 0) {
		LOG_ERROR("ext4: indexed extent directories are not supported");
		return -1;
	}

	for (uint16 i = 0; i < eh.entries; i++) {
		struct ext4_extent ex;
		ext4_read_extent(dir, i, &ex);
		LOG_INFO("ext4: dir extent[%d] block=%d len=%d start=%d", i,
			 ex.block, ex.len, ex.start);

		for (uint16 j = 0; j < ex.len; j++) {
			ext4_probe_dir_block(fs, ex.start + j);
		}
	}

	return 0;
}

static int ext4_probe_inode_extents(const char *path,
				    const struct ext4_inode *inode)
{
	struct ext4_extent_header eh;

	LOG_INFO("ext4: %s inode mode=%x size=%d blocks=%d flags=%x", path,
		 inode->mode, inode->size, inode->blocks_lo, inode->flags);

	if (ext4_read_extent_header(inode, &eh) < 0) {
		return -1;
	}

	LOG_INFO("ext4: %s extent magic=%x entries=%d max=%d depth=%d", path,
		 eh.magic, eh.entries, eh.max, eh.depth);

	if (eh.depth != 0) {
		LOG_ERROR("ext4: %s indexed extents are not supported yet",
			  path);
		return -1;
	}

	for (uint16 i = 0; i < eh.entries; i++) {
		struct ext4_extent ex;
		ext4_read_extent(inode, i, &ex);
		LOG_INFO("ext4: %s extent[%d] block=%d len=%d start=%d", path,
			 i, ex.block, ex.len, ex.start);
	}

	return 0;
}

static void ext4_probe_read_file(struct ext4_fs *fs, const char *path,
				 uint64 offset, uint32 len)
{
	struct ext4_inode inode;
	uint8 type;

	if (ext4_lookup_path(fs, path, &inode, &type) < 0) {
		LOG_ERROR("ext4: lookup %s failed", path);
		return;
	}

	if (type != EXT4_FT_REG_FILE) {
		LOG_ERROR("ext4: %s is not a regular file, type=%d", path,
			  type);
		return;
	}

	uint8 buf[128];

	if (len > sizeof(buf)) {
		LOG_ERROR("ext4: probe read too large: %d", len);
		return;
	}

	int n = ext4_read_file(fs, &inode, offset, buf, len);
	if (n < 0) {
		LOG_ERROR("ext4: read %s failed", path);
		return;
	}

	LOG_INFO("ext4: read %s offset=%d len=%d got=%d", path, offset, len, n);
	ext4_dump_bytes(path, buf, n);
}

// Temporary boot-time probe used while bringing up the EXT4 reader.
int ext4_probe(uint32 dev)
{
	if (ext4_mount_root(dev) < 0) {
		LOG_ERROR("ext4: probe failed");
		return -1;
	}

	struct ext4_fs *fs = ext4_get_root_fs();
	LOG_INFO("ext4: dev %d detected", dev);
	LOG_INFO("ext4: block_size=%d blocks=%d inodes=%d", fs->block_size,
		 fs->blocks_count, fs->inodes_count);
	LOG_INFO("ext4: inode_size=%d desc_size=%d", fs->inode_size,
		 fs->desc_size);
	uint64 inode_table_block;
	ext4_read_inode_table_block(fs, 0, &inode_table_block);
	LOG_INFO("ext4: group 0 inode_table=%d", inode_table_block);

	struct ext4_inode root;
	if (ext4_read_inode(fs, EXT4_ROOT_INO, &root) == 0) {
		LOG_INFO("ext4: root inode mode=%x size=%d blocks=%d flags=%x",
			 root.mode, root.size, root.blocks_lo, root.flags);
		ext4_probe_dir_inode(fs, &root);
	}

	ext4_probe_read_file(fs, "/musl/busybox", 0, 16);
	ext4_probe_read_file(fs, "/musl/busybox_cmd.txt", 0, 128);
	ext4_probe_read_file(fs, "/musl/busybox", 4090, 32);

	LOG_INFO("ext4: features compat=%x incompat=%x ro_compat=%x",
		 fs->feature_compat, fs->feature_incompat,
		 fs->feature_ro_compat);

	return 0;
}
