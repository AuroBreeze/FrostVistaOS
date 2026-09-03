
#include "kernel/fs.h"
#include "kernel/mm/kmalloc.h"
#define LOG_MODULE "EXT4 MIX"

#include "kernel/types.h"
#include "kernel/string.h"
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

/* Upper map: ext4 ino -> copied-up tmpfs node.
 * Stored separately from ext4_inode_info because icache slots are reused
 * without resetting private_data, which would alias the upper pointer. */
#define UPPER_MAX 64

struct upper_map {
	uint32 ino;
	struct vfs_inode *upper;
	int used;
};

static struct upper_map uppers[UPPER_MAX];

static struct vfs_inode *upper_find(uint32 ino)
{
	for (int i = 0; i < UPPER_MAX; i++) {
		if (uppers[i].used && uppers[i].ino == ino)
			return uppers[i].upper;
	}
	return 0;
}

static int upper_add(uint32 ino, struct vfs_inode *upper)
{
	for (int i = 0; i < UPPER_MAX; i++) {
		if (!uppers[i].used) {
			uppers[i].ino = ino;
			uppers[i].upper = upper;
			uppers[i].used = 1;
			return 0;
		}
	}
	return -1;
}

static void upper_remove(uint32 ino)
{
	for (int i = 0; i < UPPER_MAX; i++) {
		if (uppers[i].used && uppers[i].ino == ino) {
			uppers[i].used = 0;
			return;
		}
	}
}

/**
 * mix_destroy_tmpfs_node - tear down a fresh tmpfs node that was never
 * linked anywhere (its icache slot has count==1, only this caller holds it).
 * */
static void mix_destroy_tmpfs_node(struct vfs_inode *up)
{
	struct tmpfs_inode *ti = (struct tmpfs_inode *) up->private_data;
	struct tmpfs_dir_entry *de = (ti != 0) ? ti->dir : 0;

	up->nlinks = 0;		 /* make destroy_inode recycle it */
	tmpfs_destroy_inode(up); /* frees blocks + tmpfs_inode + slot */
	if (de != 0)
		kmfree(de);
}

static int mix_is_tmpfs(struct vfs_inode *inode); /* fwd: defined below */

/**
 * mix_vfs_read - overlay-aware read
 *
 * tmpfs nodes delegate to tmpfs_vfs_read. An ext4 lower node that was
 * copied up (write/truncate) is switched to its upper node here, so reads
 * return the modified content instead of the stale lower file.
 *
 * Return: bytes read, or -1 on error.
 * */
int mix_vfs_read(struct file *f, uint8 *buffer, uint32 size)
{
	struct vfs_inode *node;
	if (f == 0 || (node = f->node) == 0 || buffer == 0)
		return -1;

	if (mix_is_tmpfs(node))
		return tmpfs_vfs_read(f, buffer, size);

	struct vfs_inode *up = upper_find(node->ino);
	if (up != 0) {
		struct vfs_inode *vip =
		    tmpfs_fill_vfs_inode(up->ino, up->private_data, up->type);
		if (vip == 0)
			return -1;
		put_inode(node, 0);
		f->node = vip;
		return tmpfs_vfs_read(f, buffer, size);
	}
	return ext4_vfs_read(f, buffer, size);
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

	/* "." resolves to the directory itself (get_inode adds a reference).
	 * ".." is left to the fallback: ext4 dirs resolve it through their
	 * on-disk ".." entry; tmpfs dirs do not track parents and fail. */
	if (namecmp(name, ".") == 0)
		return get_inode(dir->dev, dir->ino, 1);

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

	struct vfs_inode *lower = ext4_vfs_lookup(dir, name, offset);
	if (lower != 0) {
		/* keep the basename in ext4_inode_info: copy-up (write) uses
		 * it. Refreshed on every lookup, so icache slot reuse cannot
		 * alias it to a stale name (unlike vfs_inode->name). */
		struct ext4_inode_info *info = lower->private_data;
		if (info != 0) {
			strncpy(info->name, name, sizeof(info->name) - 1);
			info->name[sizeof(info->name) - 1] = '\0';
		}

		/* if this lower inode was copied up, return its upper node */
		struct vfs_inode *up = upper_find(lower->ino);
		if (up != 0) {
			struct vfs_inode *vip = tmpfs_fill_vfs_inode(
			    up->ino, up->private_data, up->type);
			put_inode(lower, 0);
			return vip;
		}
	}
	return lower;
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
	int r;
	while (1) {
		r = ext4_vfs_readdir(f, dirent);
		if (r != 1)
			break;

		/* skip names already covered by the overlay segment
		 * (a real upper entry was emitted there, a whiteout hides
		 * the lower entry entirely) */
		if (overlay_find(f->node, dirent->name) != 0)
			continue;

		/* a copied-up lower file is shown as its upper node */
		struct vfs_inode *up = upper_find(dirent->ino);
		if (up != 0) {
			dirent->ino = up->ino;
			dirent->type = up->type;
		}
		break;
	}
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
		mix_destroy_tmpfs_node(root);
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

	if (size != 0)
		return -1; /* the VFS only requests truncation to zero */

	/* ext4 lower file: make sure an upper node exists, then truncate it
	 * (tmpfs_vfs_truncate frees the old blocks and clears size) */
	struct vfs_inode *up = upper_find(node->ino);
	if (up == 0) {
		struct ext4_inode_info *info = node->private_data;
		up = mix_create_tmpfs_root(info->name, VFS_FILE);
		if (up == 0)
			return -1;
		if (upper_add(node->ino, up) < 0) {
			mix_destroy_tmpfs_node(up);
			return -1;
		}
	}
	return tmpfs_vfs_truncate(up, size);
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
			if (entry->root->type == VFS_DIR) {
				LOG_WARN("mix_vfs_unlink: cannot unlink a "
					 "directory");
				return -1;
			}
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

	/* if this lower file was copied up, tear down the upper node and
	 * drop it from the upper map (whiteout then hides the lower) */
	uint32 ino = lower->ino;
	struct vfs_inode *up = upper_find(ino);
	if (up != 0) {
		upper_remove(ino);
		struct tmpfs_inode *ti =
		    (struct tmpfs_inode *) up->private_data;
		struct tmpfs_dir_entry *de = (ti != 0) ? ti->dir : 0;
		if (ti != 0) {
			ti->nlinks--;
			up->nlinks = ti->nlinks; /* sync slot copy */
		}
		/* drops the upper-map reference; contents are freed by
		 * destroy_inode when nlinks==0 and the last open fd closes */
		tmpfs_destroy_inode(up);
		if (de != 0)
			kmfree(de);
	}
	put_inode(lower, 0);
	return overlay_add(0, dir, name, 1);
}

/**
 * mix_vfs_write - overlay-aware write with copy-up
 *
 * tmpfs nodes delegate to tmpfs_vfs_write. For a read-only ext4 lower file
 * the first write copies the whole lower content into a fresh tmpfs node
 * and records it in the upper map (by ext4 ino), then switches this file
 * to the upper node so all later read/write/close go through tmpfs.
 *
 * Return: bytes written, or -1 on error.
 * */
int mix_vfs_write(struct file *f, uint8 *buffer, uint32 size)
{
	struct vfs_inode *node;
	if (f == 0 || (node = f->node) == 0 || buffer == 0)
		return -1;

	/* tmpfs node: plain write */
	if (mix_is_tmpfs(node))
		return tmpfs_vfs_write(f, buffer, size);

	/* ext4 lower file: copy-up on first write */
	struct ext4_inode_info *info = node->private_data;
	if (info == 0)
		return -1;

	if (upper_find(node->ino) == 0) {
		struct vfs_inode *up =
		    mix_create_tmpfs_root(info->name, VFS_FILE);
		if (up == 0)
			return -1;

		struct ext4_fs *fs = ext4_get_root_fs();
		if (fs == 0) {
			mix_destroy_tmpfs_node(up);
			return -1;
		}

		/* copy the whole lower file into the upper node */
		struct file uf = {0};
		uf.type = FILE_VFS_NODE;
		uf.node = up;

		uint8 tmp[PGSIZE];
		uint64 off = 0;
		while (off < node->size) {
			uint64 want = node->size - off;
			uint64 len = want < sizeof(tmp) ? want : sizeof(tmp);
			int n = ext4_read_file(fs, &info->disk_inode, off, tmp,
					       len);
			if (n <= 0) {
				mix_destroy_tmpfs_node(up);
				return -1;
			}
			uf.offset = off;
			if (tmpfs_vfs_write(&uf, tmp, n) != n) {
				mix_destroy_tmpfs_node(up);
				return -1;
			}
			off += n;
		}

		if (upper_add(node->ino, up) < 0) {
			mix_destroy_tmpfs_node(up);
			return -1;
		}
	}

	/* from now on this file lives in tmpfs; take our own reference and
	 * drop the ext4 one (the upper map keeps its own reference) */
	struct vfs_inode *up = upper_find(node->ino);
	struct vfs_inode *vip =
	    tmpfs_fill_vfs_inode(up->ino, up->private_data, up->type);
	if (vip == 0)
		return -1;
	put_inode(node, 0);
	f->node = vip;
	return tmpfs_vfs_write(f, buffer, size);
}
