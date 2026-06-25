#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "ext4.h"
#include "helper.h"

static int ext4_lookup_in_dir_block(struct ext4_fs *fs, uint64 block,
				    const char *name, uint32 *ino,
				    uint8 *file_type)
{
	uint64 byte_offset = block * fs->block_size;
	uint64 cache_block = byte_offset / BSIZE;
	uint64 offset_in_cache = byte_offset % BSIZE;
	struct buf *b = bread(fs->dev, cache_block);
	uint64 offset = 0;

	while (offset < fs->block_size) {
		const uint8 *dirent = b->data + offset_in_cache + offset;
		uint32 entry_ino = ext4_read_le32(dirent + EXT4_DIRENT_INODE);
		uint16 rec_len = ext4_read_le16(dirent + EXT4_DIRENT_REC_LEN);
		uint8 name_len = *(dirent + EXT4_DIRENT_NAME_LEN);
		uint8 entry_type = *(dirent + EXT4_DIRENT_FILE_TYPE);

		if (rec_len < EXT4_DIRENT_NAME ||
		    offset + rec_len > fs->block_size) {
			brelse(b);
			return -1;
		}

		if (entry_ino != 0 && name_len <= EXT4_NAME_MAX &&
		    EXT4_DIRENT_NAME + name_len <= rec_len &&
		    ext4_name_eq(dirent + EXT4_DIRENT_NAME, name_len, name)) {
			*ino = entry_ino;
			*file_type = entry_type;
			brelse(b);
			return 0;
		}

		offset += rec_len;
	}

	brelse(b);
	return -1;
}

int ext4_lookup_in_dir(struct ext4_fs *fs, struct ext4_inode *dir,
		       const char *name, uint32 *ino, uint8 *file_type)
{
	struct ext4_extent_header eh;

	if (ext4_read_extent_header(dir, &eh) < 0) {
		return -1;
	}

	if (eh.depth != 0) {
		LOG_ERROR("ext4: indexed extent lookup is not supported");
		return -1;
	}

	for (uint16 i = 0; i < eh.entries; i++) {
		struct ext4_extent ex;
		ext4_read_extent(dir, i, &ex);

		for (uint16 j = 0; j < ex.len; j++) {
			if (ext4_lookup_in_dir_block(fs, ex.start + j, name,
						     ino, file_type) == 0) {
				return 0;
			}
		}
	}

	return -1;
}

// Resolve an absolute path from the EXT4 root directory.
//
// On success, returns the final inode snapshot, its ext4_dir_entry_2 file_type,
// and optionally the final inode number. The inode number is needed when the
// result is wrapped as a vfs_inode and cached by identity.
int ext4_lookup_path_ino(struct ext4_fs *fs, const char *path,
			 struct ext4_inode *inode, uint8 *file_type,
			 uint32 *ino)
{
	if (*path != '/')
		return -1;
	struct ext4_inode root;
	if (ext4_read_inode(fs, EXT4_ROOT_INO, &root) != 0)
		return -1;

	char name[PATH_MAX];
	struct ext4_inode cur = root;
	uint8 cur_type = EXT4_FT_DIR;
	uint32 cur_ino = EXT4_ROOT_INO;

	while ((path = skipelem((char *) path, name)) != 0) {
		uint32 next_ino;
		uint8 next_type;

		if (cur_type != EXT4_FT_DIR)
			return -1;
		if (ext4_lookup_in_dir(fs, &cur, name, &next_ino, &next_type) !=
		    0)
			return -1;
		if (ext4_read_inode(fs, next_ino, &cur) != 0)
			return -1;

		cur_ino = next_ino;
		cur_type = next_type;
	}

	*inode = cur;
	*file_type = cur_type;
	if (ino != 0) {
		*ino = cur_ino;
	}
	return 0;
}
