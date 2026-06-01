#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"

#define LOG_MODULE "BLCK"

struct bcache bcache;

// xv6
/**
 * binit - initialize the buffer cache
 *
 * Context: Initialize the block cache
 * */
void binit(void)
{
	struct buf *b;

	initlock(&bcache.bcache_lock, "bcache");

	// Create linked list of buffers
	bcache.head.prev = &bcache.head;
	bcache.head.next = &bcache.head;
	for (b = bcache.buf; b < bcache.buf + NNUM; b++) {
		b->next = bcache.head.next;
		b->prev = &bcache.head;
		b->refcnt = 0;
		b->valid = 0;
		b->dirty = 0;
		initsleeplock(&b->buf_lock, "buffer sleep");
		bcache.head.next->prev = b;
		bcache.head.next = b;
	}
}

/**
 * bget - get a block from the buffer cache
 *
 * Lock contract:
 * - Entry: caller holds no buffer sleeplocks.
 * - During lookup: acquires bcache.bcache_lock while searching or selecting a
 *   cache entry.
 * - Exit success: returns a buffer with b->buf_lock held and refcnt
 *   incremented.
 * - Ownership: caller must eventually release the buffer with brelse().
 * */
struct buf *bget(uint32 dev, uint64 blkno)
{
	struct buf *buffer;

	// Check if the pointer survived until here
	acquire(&bcache.bcache_lock);

	// Check whether this cache block exists in the current buffer
	for (buffer = bcache.head.next; buffer != &bcache.head;
	     buffer = buffer->next) {
		if (buffer->blkno == blkno && buffer->dev == dev) {
			buffer->refcnt++;
			release(&bcache.bcache_lock);
			acquiresleep(&buffer->buf_lock);
			return buffer;
		}
	}

	// Find block with no reference and that do no require updates
	// LRU need start from the tail that is the least recently used
	for (buffer = bcache.head.prev; buffer != &bcache.head;
	     buffer = buffer->prev) {
		if (buffer->refcnt == 0 && buffer->dirty == 0) {
			buffer->blkno = blkno;
			buffer->dev = dev;
			buffer->valid = 0;
			buffer->refcnt++;
			release(&bcache.bcache_lock);
			acquiresleep(&buffer->buf_lock);
			return buffer;
		}
	}
	release(&bcache.bcache_lock);

	// Not found
	return 0;
}

// xv6
// Release a locked buffer.
// Move to the head of the most-recently-used list.
/*
 * Lock contract:
 * - Entry: caller must hold b->buf_lock.
 * - Exit: releases b->buf_lock and drops one buffer reference.
 * - Ownership: caller must not access b after this call unless it obtains
 *   another reference through bget() or bread().
 */
void brelse(struct buf *b)
{
	if (!holdingsleep(&b->buf_lock))
		panic("brelse");

	releasesleep(&b->buf_lock);

	acquire(&bcache.bcache_lock);
	b->refcnt--;
	if (b->refcnt == 0) {
		// no one is waiting for it.
		b->next->prev = b->prev;
		b->prev->next = b->next;
		b->next = bcache.head.next;
		b->prev = &bcache.head;
		bcache.head.next->prev = b;
		bcache.head.next = b;
	}

	release(&bcache.bcache_lock);
}

// xv6
/**
 * bread - Get a block from the buffer cache or read from disk
 *
 * Context: First, check the cache for the data; if it isn't there, read it.
 *
 * Lock contract:
 * - Entry: caller holds no lock for the target buffer.
 * - Exit success: returns a valid buffer with b->buf_lock held.
 * - Ownership: caller must call brelse() when finished with the buffer.
 *
 * Return: a locked buf with the contents of the indicated block.
 * */
struct buf *bread(int dev, uint64 blockno)
{
	struct buf *buffer;

	// Find the block or create a new block
	buffer = bget(dev, blockno);

	// If the block is not valid, read from disk
	if (!buffer->valid) {
		virtio_disk_rw(buffer, 0);
		buffer->valid = 1;
	}

	return buffer;
}

// xv6
// Write b's contents to disk.  Must be locked.
/*
 * Lock contract:
 * - Entry: caller must hold buffer->buf_lock.
 * - Exit: leaves buffer->buf_lock held after submitting the disk write.
 * - Ownership: caller remains responsible for releasing the buffer with
 *   brelse().
 */
void bwrite(struct buf *buffer)
{
	if (!holdingsleep(&buffer->buf_lock))
		panic("bwrite: not locked");
	virtio_disk_rw(buffer, 1);
}
