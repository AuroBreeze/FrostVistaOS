
#include "kernel/fs.h"
#include "kernel/mm/kmalloc.h"
#define LOG_MODULE "EXT4 MIX"

#include "kernel/types.h"
#include "kernel/log.h"
#include "ext4.h"
#include "../tmpfs/tmpfs.h"
#include "mix.h"

static struct overlay_entry entries[OVERLAY_ENTRIES];
static struct overlay_entry *buckets[OVERLAY_HASH];

/**
 * overlay_hash - hash an (ext4 dir, name) pair into a bucket
 *
 * The hash is only used for bucketing; exact matching still compares
 * (parent, name) inside the bucket chain.
 *
 * Return: bucket index in [0, OVERLAY_HASH).
 * */
static uint32 overlay_hash(struct vfs_inode *parent, const char *name)
{
	uint32 h = (uint32) ((uint64) parent >> 4);
	h ^= (uint32) (uint64) parent;

	for (int i = 0; name[i] != '\0'; i++)
		h = (h * 33) + (uint8) name[i];

	return h & (OVERLAY_HASH - 1);
}

/**
 * overlay_find_empty - find a free slot in the entries array
 *
 * Return: slot index, or -1 if the table is full.
 * */
static int overlay_find_empty()
{
	for (int i = 0; i < OVERLAY_ENTRIES; i++) {
		if (entries[i].used == 0) {
			return i;
		}
	}
	return -1;
}

/**
 * overlay_find - look up an overlay entry by (parent, name)
 *
 * Return: matching entry, or 0 if absent.
 * */
static struct overlay_entry *overlay_find(struct vfs_inode *parent,
					  const char *name)
{
	int hash = overlay_hash(parent, name);
	for (struct overlay_entry *entry = buckets[hash]; entry != 0;
	     entry = entry->next) {
		/* skip removed slots: overlay_remove only clears ->used */
		if (entry->used == 0)
			continue;
		if (entry->parent == parent &&
		    namecmp(entry->name, name) == 0) {
			return entry;
		}
	}
	return 0;
}

/**
 * overlay_add - register an overlay entry in the hash table
 *
 * @root: the tmpfs vfs_inode backing this name under @parent
 * @whiteout: nonzero marks the entry as hiding a lower ext4 entry
 *
 * Return: 0 on success, -1 if the table is full.
 * */
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

static int overlay_remove(struct vfs_inode *parent, const char *name)
{
	int hash = overlay_hash(parent, name);
	struct overlay_entry *entry = buckets[hash];
	struct overlay_entry *prev = 0;

	while (entry != 0) {
		if (entry->used != 0 && entry->parent == parent &&
		    namecmp(entry->name, name) == 0) {
			/* detach from the bucket chain */
			if (prev == 0)
				buckets[hash] = entry->next;
			else
				prev->next = entry->next;
			entry->next = 0;
			entry->used = 0;
			return 0;
		}
		prev = entry;
		entry = entry->next;
	}

	return -1;
}

/**
 * mix_is_tmpfs - whether an inode belongs to tmpfs
 *
 * Return: 1 if inode->dev == TMPFS_DEV, else 0 (also for NULL).
 * */
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

/**
 * mix_vfs_lookup - overlay-aware lookup on an ext4 directory
 *
 * tmpfs directories are forwarded directly to tmpfs_vfs_lookup. For ext4
 * directories the overlay table is checked first (whiteout hides the lower
 * entry); a miss falls back to the read-only ext4 lookup.
 *
 * Return: the resolved vfs_inode (caller releases with put_inode), or 0.
 * */
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

/**
 * mix_create_tmpfs_root - allocate a fresh tmpfs inode + dir_entry
 *
 * Used by the overlay write path: creates the tmpfs objects backing a new
 * name under an ext4 directory. The returned vfs_inode is a filled icache
 * slot (count +1); on error the kmalloc'd objects are freed.
 *
 * Return: the filled vfs_inode, or 0 on failure.
 * */
static struct vfs_inode *mix_create_tmpfs_root(char *name, int type)
{
	struct tmpfs_inode *inode = kmalloc(sizeof(struct tmpfs_inode));
	struct tmpfs_dir_entry *dir = kmalloc(sizeof(struct tmpfs_dir_entry));
	if (inode == 0 || dir == 0) {
		if (inode != 0)
			kmfree(inode);
		if (dir != 0)
			kmfree(dir);
		return 0;
	}

	inode->type = type;
	inode->nlinks = 1;
	inode->ino = tmpfs_next_ino++;
	inode->size = 0;
	inode->dir = dir;
	memset(inode->blocks, 0, sizeof(inode->blocks));

	dir->inode = inode;
	dir->type = type;
	dir->child = 0;
	dir->next = 0;

	int len = (strlen(name) < DIRSIZ) ? strlen(name) : DIRSIZ - 1;
	memcpy(dir->name, name, len);
	dir->name[len] = '\0';

	struct vfs_inode *vfs_inode =
	    tmpfs_fill_vfs_inode(inode->ino, inode, type);
	if (vfs_inode == 0) {
		kmfree(inode);
		kmfree(dir);
		return 0;
	}
	return vfs_inode;
}

/**
 * mix_vfs_create - create a new entry, backing it in tmpfs
 *
 * tmpfs directories delegate to tmpfs_vfs_create. For ext4 directories a
 * tmpfs inode is allocated and registered in the overlay table (lower ext4
 * is never written, so create always lands in tmpfs).
 *
 * Return: 0 on success, -1 on error.
 * */
int mix_vfs_create(struct vfs_inode *dir, char *name, int mode)
{
	if (dir == 0 || dir->type != VFS_DIR || dir->private_data == 0) {
		LOG_WARN("mix_vfs_create: not a dir");
		return -1;
	}
	if (name == 0 || name[0] == '\0') {
		LOG_WARN("mix_vfs_create: bad name");
		return -1;
	}

	if (mix_is_tmpfs(dir)) {
		int ret = tmpfs_vfs_create(dir, name, mode);
		return ret;
	}

	struct overlay_entry *entry = overlay_find(dir, name);
	if (entry != 0) {
		if (entry->whiteout == 0) {
			LOG_WARN("mix_vfs_create: %s already exists", name);
			return -1;
		}
		/* A whiteout entry records "this name was deleted"; recreating
		 * the same name replaces the whiteout with a real node. */
		overlay_remove(dir, name);
	}

	struct vfs_inode *root = mix_create_tmpfs_root(name, mode);
	if (root == 0) {
		return -1;
	}

	if (overlay_add(root, dir, name, 0) < 0) {
		struct tmpfs_inode *ti =
		    (struct tmpfs_inode *) root->private_data;
		struct tmpfs_dir_entry *de = ti->dir;
		root->nlinks = 0;
		tmpfs_destroy_inode(root);
		kmfree(de);
		return -1;
	}

	return 0;
}

/**
 * mix_vfs_mkdir - create a directory through the overlay
 *
 * Reuses mix_vfs_create with VFS_DIR.
 *
 * Return: 0 on success, -1 on error.
 * */
int mix_vfs_mkdir(struct vfs_inode *dir, char *name, int mode)
{
	(void) mode;
	return mix_vfs_create(dir, name, VFS_DIR);
}

int mix_vfs_truncate(struct vfs_inode *node, uint64 size)
{
	if (node == 0 || node->type != VFS_FILE || node->private_data == 0)
		return -1;

	if (mix_is_tmpfs(node))
		return tmpfs_vfs_truncate(node, size);
	else {
		return -1;
	}
}

int mix_vfs_unlink(struct vfs_inode *dir, char *name)
{
	if (dir == 0 || dir->type != VFS_DIR || dir->private_data == 0) {
		return -1;
	}
	if (name == 0 || name[0] == '\0') {
		return -1;
	}

	for (int i = 0; i < OVERLAY_ENTRIES; i++) {
		struct overlay_entry *entry = &entries[i];
		if (entry->parent == dir && namecmp(entry->name, name) == 0 &&
		    entry->used != 0 && entry->whiteout == 0) {
			/* unlink an upper-only (tmpfs) entry */
			struct tmpfs_inode *ti =
			    (struct tmpfs_inode *) entry->root->private_data;
			ti->nlinks--;
			entry->root->nlinks = ti->nlinks; /* sync slot copy */
			if (ti->nlinks == 0) {
				overlay_remove(dir, name);
				struct tmpfs_dir_entry *de = ti->dir;
				/* count==1 && nlinks==0 -> destroy_inode
				 * frees blocks + tmpfs_inode + recycles slot */
				tmpfs_destroy_inode(entry->root);
				kmfree(de);
			}
			return 0;
		}
	}

	if (mix_is_tmpfs(dir)) {
		return tmpfs_vfs_unlink(dir, name);
	}

	/* unlink of a lower ext4 file: ext4 is read-only, so record a
	 * whiteout (root = 0, whiteout = 1) that hides the lower entry. */
	struct vfs_inode *lower = ext4_vfs_lookup(dir, name, 0);
	if (lower == 0 || lower->type == VFS_DIR) {
		if (lower != 0)
			put_inode(lower, 0);
		return -1;
	}

	return overlay_add(0, dir, name, 1);
}
