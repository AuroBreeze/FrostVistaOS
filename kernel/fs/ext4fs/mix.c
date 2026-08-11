
#define LOG_MODULE "EXT4 MIX"

#include "kernel/types.h"
#include "kernel/log.h"
#include "ext4.h"
#include "../tmpfs/tmpfs.h"
#include "mix.h"

static struct overlay_entry entries[OVERLAY_ENTRIES];
static struct overlay_entry *buckets[OVERLAY_HASH];

static uint32 overlay_hash(struct vfs_inode *parent, const char *name)
{
	uint32 h = (uint32) ((uint64) parent >> 4);
	h ^= (uint32) (uint64) parent;

	for (int i = 0; name[i] != '\0'; i++)
		h = (h * 33) + (uint8) name[i];

	return h & (OVERLAY_HASH - 1);
}

static int overlay_find_empty()
{
	for (int i = 0; i < OVERLAY_ENTRIES; i++) {
		if (entries[i].used == 0) {
			return i;
		}
	}
	return -1;
}

static struct overlay_entry *overlay_find(struct vfs_inode *parent,
					  const char *name)
{
	int hash = overlay_hash(parent, name);
	for (struct overlay_entry *entry = buckets[hash]; entry != 0;
	     entry = entry->next) {
		if (entry->parent == parent &&
		    namecmp(entry->name, name) == 0) {
			return entry;
		}
	}
	return 0;
}

static int overlay_add(struct vfs_inode *root, struct vfs_inode *parent,
		       const char *name, int whiteout)
{
	int i = overlay_find_empty();
	if (i == -1) {
		LOG_ERROR("overlay_add: no free entry");
		return -1;
	}
	struct overlay_entry *entry = &entries[i];
	entry->root = root;
	entry->parent = parent;
	entry->whiteout = whiteout;
	entry->used = 1;

	int len = (strlen(name) < DIRSIZ) ? strlen(name) : DIRSIZ - 1;
	memcpy(entry->name, name, len);
	entry->name[len] = '\0';

	int hash = overlay_hash(parent, name);
	if (buckets[hash] == 0) {
		entry->next = 0;
	} else {
		entry->next = buckets[hash];
	}
	buckets[hash] = entry;
	return 0;
}

static int mix_is_tmpfs(struct vfs_inode *inode)
{
	if (inode == 0)
		return 0;

	return inode->dev == TMPFS_DEV;
}
/* Scan entries[] from slot *offset for the next entry of @dir.
 * Whiteout entries are skipped (they only hide lower entries).
 * *offset is advanced past the returned slot; entries added before the
 * current slot during a scan are not reported (acceptable for readdir). */
static int overlay_readdir_at(struct vfs_inode *dir, struct vfs_dirent *dirent,
			      uint64 *offset)
{
	for (int i = *offset; i < OVERLAY_ENTRIES; i++) {
		struct overlay_entry *entry = &entries[i];
		if (entry->parent != dir || entry->used == 0 ||
		    entry->whiteout != 0)
			continue;
		dirent->ino = entry->root->ino;
		dirent->type = entry->root->type;
		strcpy(dirent->name, entry->name);

		*offset = i + 1;
		return 1;
	}

	return 0;
}

struct vfs_inode *mix_vfs_lookup(struct vfs_inode *dir, char *name,
				 uint32 *offset)
{
	if (dir == 0 || dir->type != VFS_DIR || dir->private_data == 0) {
		return 0;
	}

	if (mix_is_tmpfs(dir)) {
		return tmpfs_vfs_lookup(dir, name, offset);
	}

	struct overlay_entry *entry = overlay_find(dir, name);
	if (entry != 0) {
		struct vfs_inode *tmp_inode = entry->root;
		if (entry->whiteout || entry->used == 0) {
			return 0;
		}
		// `put_inode` might eventually be used to release it, so a new
		// initialization is used.
		return tmpfs_fill_vfs_inode(entry->root->ino,
					    entry->root->private_data,
					    entry->root->type);
	}

	return ext4_vfs_lookup(dir, name, offset);
}

int mix_vfs_readdir(struct file *f, struct vfs_dirent *dirent)
{
	if (f == 0 || f->node == 0 || dirent == 0) {
		LOG_ERROR("mix_vfs_readdir: bad args f=%p node=%p dirent=%p",
			  (void *) f, f ? (void *) f->node : 0,
			  (void *) dirent);
		return -1;
	}
	if (f->type != FILE_VFS_NODE || f->node == 0 ||
	    f->node->type != VFS_DIR) {
		LOG_ERROR("mix_vfs_readdir: not a dir ftype=%d node_type=%d",
			  f->type, f->node ? f->node->type : -1);
		return -1;
	}

	if (mix_is_tmpfs(f->node)) {
		return tmpfs_vfs_readdir(f, dirent);
	}
	/* offset segment convention:
	 *  [0, OVERLAY_ENTRIES)  : overlay segment, f->offset indexes entries[]
	 *  [OVERLAY_ENTRIES, oo) : ext4 segment, ext4 offset = f->offset -
	 *                          OVERLAY_ENTRIES
	 *  The two segments use different semantics; OVERLAY_ENTRIES is only
	 *  a boundary between them. */
	uint64 off = f->offset;
	if (off < OVERLAY_ENTRIES) {
		int ret = overlay_readdir_at(f->node, dirent, &off);
		if (ret == 1) {
			f->offset = off; /* write back slot index */
			return 1;
		}
		off = OVERLAY_ENTRIES; /* overlay segment finished */
	}

	f->offset = off - OVERLAY_ENTRIES; /* ext4 segment start */
	int r = ext4_vfs_readdir(f, dirent);
	if (r == 1)
		f->offset = OVERLAY_ENTRIES + f->offset; /* ext4 write back */
	return r;
}
