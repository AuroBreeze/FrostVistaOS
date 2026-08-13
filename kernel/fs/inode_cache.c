
#define LOG_MODULE "ICAC"

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
		inc->pd_owned = 0;
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
struct vfs_inode *get_inode(uint32 dev, uint32 ino, int alloc)
{
	struct vfs_inode *ip;
	acquire(&icache.lock);

	// Reuse a cached slot already holding this inode, regardless of its
	// reference count. A slot with count==0 still owns its private_data
	// page (unless put_inode freed it when nlinks dropped to 0), so
	// reallocate only when the page is gone.
	for (ip = &icache.inodes[0]; ip < &icache.inodes[NINODES]; ip++) {
		if (ip->ino == ino && ip->dev == dev) {
			if (ip->count > 0)
				ip->count++;
			else {
				ip->count = 1;
				if (ip->private_data == 0 && alloc) {
					ip->private_data = kalloc();
					ip->pd_owned = 1;
				}
			}
			release(&icache.lock);
			LOG_TRACE("get_inode: hit ino %d", ino);
			return ip;
		}
	}

	LOG_TRACE("get_inode: miss ino %d", ino);
	// True miss: pick a free slot (count==0) from the LRU tail. Reuse its
	// private_data page if one is still attached AND belongs to the same
	// device (the filesystem will refresh the contents); allocate only when
	// the page is gone, so the old page is never leaked by overwriting its
	// pointer.
	//
	// When the slot is taken over by a DIFFERENT device, its private_data
	// belongs to another filesystem and must not be handed to the new
	// owner: the new filesystem would misread it and write through it,
	// corrupting the old filesystem's object (e.g. easyfs_fill_vfs_inode
	// memmove()'s block numbers into a tmpfs_inode).
	//
	// Ownership is tracked by pd_owned:
	//   - pd_owned == 1: the cache kalloc'd the page, so the cache must
	//     kfree it here (no leak) before letting the new owner reallocate.
	//   - pd_owned == 0: a filesystem (tmpfs) owns private_data (a
	//     tmpfs_inode kept alive by its dirent chain), so the cache only
	//     drops the pointer and never frees it.
	for (ip = icache.head.prev; ip != &icache.head; ip = ip->prev) {
		if (ip->count == 0) {
			if (ip->dev != dev) {
				if (ip->pd_owned && ip->private_data)
					kfree(ip->private_data);
				ip->private_data = 0;
				ip->pd_owned = 0;
			}

			ip->ino = ino;
			ip->count = 1;
			ip->dev = dev;

			if (ip->private_data == 0 && alloc) {
				ip->private_data = kalloc();
				ip->pd_owned = 1;
			}

			release(&icache.lock);
			return ip;
		}
	}

	LOG_ERROR("get_inode: no inodes (ino %d)", ino);

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
void put_inode(struct vfs_inode *ip, int free)
{
	acquire(&icache.lock);
	if (ip->count == 1 && ip->nlinks == 0) {
		// acquiresleep(&ip->lock);
		ip->count = 0;
		// Move this recently freed inode to the MRU position
		// (head.next)
		ip->prev->next = ip->next;
		ip->next->prev = ip->prev;
		if (free)
			kfree(ip->private_data);
		ip->private_data = 0;
		ip->pd_owned = 0;
		ip->next = icache.head.next;
		ip->prev = &icache.head;
		icache.head.next->prev = ip;
		icache.head.next = ip;
		release(&icache.lock);

		// FIXME: clean the inode
		//
		// itrunc(ip);
		// ip->type = 0;
		// ip->ino = 0;
		// iupdate(ip);

		// releasesleep(&ip->lock);
		return;
	} else if (ip->count == 0) {
		LOG_ERROR("put_inode: inode %d not found", ip->ino);
		release(&icache.lock);
		return;
	} else {
		ip->count--;
	}
	release(&icache.lock);
}
