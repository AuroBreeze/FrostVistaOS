#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/icache.h"
#include "kernel/log.h"

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
	(void) dev;

	struct vfs_inode *ip;
	struct easyfs_inode_info *info;
	acquire(&icache.lock);

	// Check if the pointer survived until here
	for (ip = &icache.inodes[0]; ip < &icache.inodes[NINODES]; ip++) {
		// info = (struct easyfs_inode_info *) ip->private_data;
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

			ip->private_data = kalloc();

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

		// itrunc(ip);
		// ip->type = 0;
		// ip->ino = 0;
		// iupdate(ip);

		releasesleep(&ip->lock);
		return;
	} else {
		ip->count--;
	}
	release(&icache.lock);
}
