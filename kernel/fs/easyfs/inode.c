#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/string.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "easyfs.h"

/*
 * Reference contract:
 * - Entry: caller holds no inode locks.
 * - Exit success: allocates an on-disk inode slot and returns a referenced,
 *   unlocked inode from get_inode().
 * - Failure: releases any buffer locks acquired internally and returns 0.
 * - Ownership: caller must eventually release the returned inode with
 *   put_inode(), or by locking it and then calling iunlockput().
 */
struct vfs_inode *ialloc(uint32 dev)
{
	// inode bitmap
	uint32 ino;

	struct buf *buf = bread(dev, INOBLK_BMIP);
	for (int i = 0; i < BSIZE; i++) {
		// All slots are currently filled
		if (buf->data[i] == 0xFF)
			continue;
		int temp = 1;
		// Find unused bits
		for (int shift = 0; shift < 8; shift++) {
			temp = 1 << shift;
			if (!(buf->data[i] & temp)) {
				// Set this bit to 1
				buf->data[i] |= temp;

				ino = (i * 8) + shift;
				if (ino == 0) {
					buf->data[i] &= ~temp;
					bwrite(buf);
					continue;
				}
				goto handle_found;
			}
		}
		LOG_WARN("ialloc: out of space");
	}
	LOG_WARN("ialloc: No available space");
	brelse(buf);
	return 0;

handle_found:
	bwrite(buf);
	brelse(buf);

	return get_inode(EASYFS_DEV, ino, 1);
}

void ifree(uint32 dev, uint32 ino)
{
	if (ino == 0)
		return;
	struct buf *buf = bread(dev, INOBLK_BMIP);
	int i = ino / 8;
	int shift = ino % 8;
	buf->data[i] &= ~(1 << shift);
	bwrite(buf);
	brelse(buf);

	uint32 data_block = (ino / 64) + INODE_BLOCK;
	uint32 offset = ino % 64;
	struct buf *data_buf = bread(dev, data_block);
	struct disk_inode *inode =
	    (struct disk_inode *) data_buf->data + offset;
	memset(inode, 0, sizeof(struct disk_inode));
	bwrite(data_buf);
	brelse(data_buf);
}

// xv6
// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk.
// Caller must hold ip->lock.
/*
 * Lock contract:
 * - Entry: caller must hold ip->lock.
 * - Exit: leaves ip->lock held after copying in-memory inode metadata to the
 *   on-disk inode.
 * - Ownership: does not change the inode reference count.
 */
void iupdate(struct vfs_inode *ip)
{
	struct buf *bp;
	struct disk_inode *dip;
	uint32 blkno;
	uint32 offset;

	blkno = INODE_BLOCK + (ip->ino / 64);
	offset = ip->ino % 64;

	bp = bread(0, blkno);

	dip = (struct disk_inode *) bp->data + offset;
	dip->type = ip->type;
	dip->nlinks = ip->nlinks;
	dip->size = ip->size;
	struct easyfs_inode_info *ei =
	    (struct easyfs_inode_info *) ip->private_data;
	memmove(dip->blocks, ei->blocks, sizeof(ei->blocks));

	bwrite(bp);
	brelse(bp);
}

static int easyfs_vfs_read(struct file *f, uint8 *buffer, uint32 size)
{
	if (f == 0 || f->node == 0 || buffer == 0) {
		return -1;
	}

	vfs_ilock(f->node);
	int n = easyfs_read_inode(f->node, 0, (uint64) buffer, f->offset, size);
	vfs_iunlock(f->node);

	return n;
}

static int easyfs_vfs_write(struct file *f, uint8 *buffer, uint32 size)
{
	if (f == 0 || f->node == 0 || buffer == 0) {
		return -1;
	}

	vfs_ilock(f->node);
	int n =
	    easyfs_write_inode(f->node, 0, (uint64) buffer, f->offset, size);
	vfs_iunlock(f->node);

	return n;
}

static int easyfs_vfs_stat(struct vfs_inode *node, struct stat *st)
{
	if (node == 0 || st == 0)
		return -1;

	st->addr = 0;
	st->type = node->type;
	st->nlink = node->nlinks;
	st->size = node->size;

	return 0;
}

/**
 * easyfs_vfs_readdir - Read the contents of the directory pointed to by node
 *
 * Context:
 *  Read the file f and copy the next directory entry into dirent
 *
 * Returns: if the dirent is read successfully, return 1, if the dirent is
 * empty, return 0, for error, return -1
 * */
static int easyfs_vfs_readdir(struct file *f, struct vfs_dirent *dirent)
{

	if (f == 0 || f->node == 0 || dirent == 0) {
		LOG_ERROR("easyfs_vfs_readdir: bad args f=%p node=%p dirent=%p",
			  (void *) f, f ? (void *) f->node : 0,
			  (void *) dirent);
		return -1;
	}
	if (f->type != FILE_VFS_NODE || f->node == 0 ||
	    f->node->type != VFS_DIR) {
		LOG_ERROR("easyfs_vfs_readdir: not a dir ftype=%d node_type=%d",
			  f->type, f->node ? f->node->type : -1);
		return -1;
	}

	uint32 off = f->offset;
	struct disk_dir_entry de;
	struct vfs_inode *inode;
	struct vfs_inode *dp = f->node;

	acquiresleep(&dp->lock);
	// Look for an empty dirent.
	for (; off < dp->size; off += sizeof(de)) {
		if (easyfs_read_inode(dp, 0, (uint64) &de, off, sizeof(de)) !=
		    sizeof(de))
			panic("dirlink read");
		if (de.inode_num == 0)
			continue;

		memcpy(dirent->name, de.name, sizeof(de.name));
		dirent->name[sizeof(de.name) - 1] = '\0';

		dirent->ino = de.inode_num;
		if ((inode = easyfs_get_vfs_inode(de.inode_num)) == 0) {
			LOG_ERROR("easyfs_vfs_readdir: get inode %d failed",
				  de.inode_num);
			releasesleep(&dp->lock);
			return -1;
		} else {
			dirent->type = inode->type;
			vfs_iput(inode);
		}
		f->offset = off + sizeof(de);
		releasesleep(&dp->lock);
		return 1;
	}
	releasesleep(&dp->lock);
	return 0;
}

static struct vfs_inode_ops easyfs_inode_ops = {
    .lookup = easyfs_vfs_lookup,
    .stat = easyfs_vfs_stat,
    .create = easyfs_vfs_create,
    .truncate = easyfs_itrunc,
    .unlink = easyfs_vfs_unlink,
    .mkdir = easyfs_vfs_mkdir,
};

static struct vfs_file_ops easyfs_file_ops = {
    .read = easyfs_vfs_read,
    .write = easyfs_vfs_write,
    .readdir = easyfs_vfs_readdir,
    .lseek = 0,
    .close = 0,
};

static void easyfs_destroy_inode(struct vfs_inode *ip)
{
	put_inode(ip, 1);
}

static struct vfs_superblock_ops easyfs_superblock_ops = {
    .alloc_inode = 0,
    .destroy_inode = easyfs_destroy_inode,
    .write_super = 0,
};

struct vfs_inode *easyfs_fill_vfs_inode(uint32 ino, struct disk_inode *inode,
					uint8 file_type)
{
	struct vfs_inode *vip = get_inode(EASYFS_DEV, ino, 1);
	if (!vip)
		return 0;

	struct easyfs_inode_info *info = vip->private_data;

	if (!info) {
		LOG_ERROR("easyfs_fill_vfs_inode: ino %d private_data is NULL",
			  ino);
		put_inode(vip, 1);
		return 0;
	}

	memmove(info->blocks, inode->blocks, sizeof(info->blocks));

	// The count is managed by `inode_cache`.
	vip->dev = EASYFS_DEV;
	vip->ino = ino;
	vip->nlinks = inode->nlinks;
	vip->type = file_type;
	vip->size = inode->size;
	vip->ops = &easyfs_inode_ops;
	vip->default_f_ops = &easyfs_file_ops;
	vip->private_data = info;
	vip->sb = easyfs_get_root_sb();
	vip->sb->ops = &easyfs_superblock_ops;

	initsleeplock(&vip->lock, "easyfs inode");

	return vip;
}

int easyfs_vfs_mkdir(struct vfs_inode *dir, char *name, int mode)
{
	if (dir == 0 || name == 0 || name[0] == '\0')
		return -1;
	if (mode != 0) {
		LOG_WARN("easyfs_vfs_mkdir: mode not supported");
		return -1;
	}

	easyfs_ilock(dir);
	struct vfs_inode *existing = easyfs_vfs_lookup(dir, name, 0);
	if (existing != 0) {
		put_inode(existing, 1);
		easyfs_iunlock(dir);
		return -1;
	}
	struct vfs_inode *ip = ialloc(EASYFS_DEV);
	if (ip == 0) {
		easyfs_iunlock(dir);
		return -1;
	}

	easyfs_ilock(ip);
	ip->type = VFS_DIR;
	ip->nlinks = 1;
	iupdate(ip);

	if (dirlink(ip, ".", ip->ino) < 0 || dirlink(ip, "..", dir->ino) < 0 ||
	    dirlink(dir, name, ip->ino) < 0) {
		ip->nlinks = 0;
		iupdate(ip);
		easyfs_iunlockput(ip);
		easyfs_iunlock(dir);
		return -1;
	}

	dir->nlinks++;
	iupdate(dir);

	easyfs_iunlockput(ip);
	easyfs_iunlock(dir);
	return 0;
}

int easyfs_vfs_create(struct vfs_inode *dir, char *path, int mode)
{
	if (dir == 0 || path == 0 || path[0] == '\0')
		return -1;

	easyfs_ilock(dir);
	struct vfs_inode *existing = easyfs_vfs_lookup(dir, path, 0);
	if (existing != 0) {
		put_inode(existing, 1);
		easyfs_iunlock(dir);
		return -1;
	}

	struct vfs_inode *ip = ialloc(EASYFS_DEV);
	if (ip == 0) {
		easyfs_iunlock(dir);
		return -1;
	}

	easyfs_ilock(ip);
	ip->type = mode;
	ip->nlinks = 1;
	iupdate(ip);
	easyfs_iunlock(ip);

	if (dirlink(dir, path, ip->ino) < 0) {
		easyfs_ilock(ip);
		ip->nlinks = 0;
		iupdate(ip);
		easyfs_iunlockput(ip);
		easyfs_iunlock(dir);
		return -1;
	}

	put_inode(ip, 1);
	easyfs_iunlock(dir);
	return 0;
}

int easyfs_vfs_unlink(struct vfs_inode *dir, char *name)
{
	struct vfs_inode *ip;
	struct disk_dir_entry de;
	uint32 off;

	if (dir == 0 || name == 0 || name[0] == '\0') {
		return -1;
	}

	easyfs_ilock(dir);

	// cannot unlink "." or ".."
	if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
		goto fail;

	if ((ip = easyfs_vfs_lookup(dir, name, &off)) == 0) {
		goto fail;
	}

	easyfs_ilock(ip);
	if (ip->type == VFS_DIR) {
		easyfs_iunlockput(ip);
		LOG_WARN("unlink: directory unlink is not supported");
		goto fail;
	}

	memset(&de, 0, sizeof(de));
	if ((easyfs_write_inode(dir, 0, (uint64) &de, off, sizeof(de))) !=
	    sizeof(de)) {
		panic("unlink: writei failed");
	}

	ip->nlinks--;
	if (ip->nlinks == 0) {
		easyfs_itrunc(ip, 0);
		ifree(EASYFS_DEV, ip->ino);
	}
	iupdate(ip);
	easyfs_iunlockput(ip);
	easyfs_iunlock(dir);

	return 0;
fail:
	easyfs_iunlock(dir);
	return -1;
}
