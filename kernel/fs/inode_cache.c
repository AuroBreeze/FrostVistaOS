#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/easyfs.h"
#include "kernel/fs.h"
#include "kernel/icache.h"
#include "kernel/log.h"
#include "kernel/types.h"

struct inode_cache icache;

/**
 * icache_init - initialize the inode cache
 * */
void icache_init(void)
{

	initlock(&icache.lock, "icache lock");

	struct vfs_inode *inc;
	// Create linked list of buffers
	icache.head.prev = &icache.head;
	icache.head.next = &icache.head;
	for (inc = icache.inodes; inc < icache.inodes + NINODES; inc++) {
		inc->next = icache.head.next;
		inc->prev = &icache.head;
		inc->count = 0;
		initsleeplock(&inc->lock, "inode lock");
		icache.head.next->prev = inc;
		icache.head.next = inc;
	}
	LOG_TRACE("icache_init done");
}

/**
 * get_inode - search ino in the inode cache
 *
 * Reference contract:
 * - Entry: caller must not hold the target inode sleeplock.
 * - Exit success: returns an unlocked inode with its reference count
 *   incremented.
 * - Ownership: caller must eventually release the reference with
 *   put_inode(), or by locking it and then calling iunlockput().
 *
 * Return: pointer to the inode
 * */
struct vfs_inode *get_inode(uint32 dev, uint32 ino)
{
	struct vfs_inode *ip;
	struct easyfs_inode_info *info;
	acquire(&icache.lock);

	// Check if the pointer survived until here
	for (ip = &icache.inodes[0]; ip < &icache.inodes[NINODES]; ip++) {
		info = (struct easyfs_inode_info *) ip->private_data;
		if (ip->ino == ino && ip->count > 0) {
			ip->count++;
			release(&icache.lock);
			LOG_TRACE("get_inode: hit ino %d", ino);
			return ip;
		}
	}

	LOG_TRACE("get_inode: miss ino %d", ino);
	// LRU need start from the tail
	for (ip = icache.head.prev; ip != &icache.head; ip = ip->prev) {
		if (ip->count == 0) {
			ip->ino = ino;
			ip->count = 1;

			ip->private_data =
			    (struct easyfs_inode_info *) kalloc();

			release(&icache.lock);
			return ip;
		}
	}

	LOG_ERROR("get_inode: no inodes");

	release(&icache.lock);
	return 0;
}

/**
 * put_inode - put inode into the inode cache
 *
 * Reference contract:
 * - Entry: caller must own one reference to ip.
 * - Entry: caller should not hold ip->lock; use iunlockput() when a locked
 *   inode must be unlocked and released together.
 * - Exit: drops one reference. If this was the last reference and the inode
 *   has no links, the inode may be truncated, cleared, and recycled.
 * - Ownership: caller must not use ip after this call unless it owns another
 *   reference.
 * */
void put_inode(struct vfs_inode *ip)
{
	acquire(&icache.lock);
	if (ip->count == 1 && ip->nlinks == 0) {
		acquiresleep(&ip->lock);
		ip->count = 0;
		// Move this recently freed inode to the MRU position
		// (head.next)
		ip->prev->next = ip->next;
		ip->next->prev = ip->prev;
		kfree(ip->private_data);
		ip->next = icache.head.next;
		ip->prev = &icache.head;
		icache.head.next->prev = ip;
		icache.head.next = ip;
		release(&icache.lock);

		itrunc(ip);
		ip->type = 0;
		ip->ino = 0;
		iupdate(ip);

		releasesleep(&ip->lock);
		return;
	} else {
		ip->count--;
	}
	release(&icache.lock);
}

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
	uint32 data_block;
	uint32 offset;
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
				data_block = (ino / 64) + INODE_BLOCK;
				offset = ino % 64;
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
	struct buf *data_buf = bread(dev, data_block);

	struct disk_inode *inode =
	    (struct disk_inode *) data_buf->data + offset;
	memset(inode, 0, sizeof(struct disk_inode));

	bwrite(data_buf);
	brelse(data_buf);
	LOG_TRACE("Allocated Inode %d", data_block);

	return get_inode(EASYFS_DEV, ino);
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
