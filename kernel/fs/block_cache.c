#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/easyfs.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "kernel/types.h"

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
void bwrite(struct buf *buffer)
{
	if (!holdingsleep(&buffer->buf_lock))
		panic("bwrite: not locked");
	virtio_disk_rw(buffer, 1);
}

/**
 * balloc - Allocate a free data block
 *
 * Context: Search for free bits in the data bitmap on page 3 and allocate the
 * corresponding space in the data area.
 *
 * Return: Returns the block address of the corresponding data area found
 * */
uint32 balloc()
{
	struct buf *buf;
	uint32 data_block;

	// 3 is the Data Bitmap
	// TODO: Eliminate the Magic Number
	buf = bread(0, 3);
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

				// TODO: Eliminate the Magic Number
				// 11 is the data block area
				data_block = (i * 8) + shift + 11;
				goto handle_found;
			}
		}
		LOG_WARN("balloc: out of space");
	}
	LOG_WARN("balloc: No available bits found");
	brelse(buf);
	return -1;

handle_found:
	bwrite(buf);
	brelse(buf);
	struct buf *data_buf = bread(0, data_block);
	memset((void *) data_buf->data, 0, BSIZE);
	bwrite(data_buf);
	brelse(data_buf);
	LOG_TRACE("Allocated block %d", data_block);

	return data_block;
};

/**
 * bfree - Free a data block
 *
 * Context: Mark the corresponding bit in the data bitmap as free and zero out
 * the data area
 * */
void bfree(uint32 dev, uint32 block_num)
{
	if (block_num < 11 || block_num >= 1000) {
		panic("bfree: block number out of range");
	}

	uint32 offset = block_num - 11;
	uint32 byte_idx = offset / 8;
	uint32 bit_idx = offset % 8;

	// 3 is the Data Bitmap
	// TODO: Eliminate the Magic Number
	struct buf *buf = bread(dev, 3);
	int mask = (1 << bit_idx);

	// Check if the block is already free
	if (buf->data[byte_idx] & mask) {
		panic("bfree: block already free");
	}

	buf->data[byte_idx] |= mask;
	bwrite(buf);
	brelse(buf);
	LOG_TRACE("Freed block %d", block_num);

	// Zero out the freed block for security and easier debugging
	struct buf *data_buf = bread(dev, block_num);
	memset((void *) data_buf->data, 0, BSIZE);
	bwrite(data_buf);
	brelse(data_buf);
	LOG_TRACE("Zeroed out block %d", block_num);
}

/**
 * xv6
 * bmap - Find the disk block content of a file
 *
 * Context: Perform a lookup using the private data stored in the file system
 * specified by `private`
 *
 * @block_num: The block number is the index of the block in the block
 * array(blocks[12])
 * */
// TODO: For now, we are using only 12 blocks and not using indirect addresses.
uint bmap(struct vfs_inode *ip, uint32 block_num)
{
	if (block_num < NDIRECT) {
		uint32 addr;
		struct easyfs_inode_info *ei =
		    (struct easyfs_inode_info *) ip->private_data;
		if ((addr = ei->blocks[block_num]) == 0) {
			addr = balloc();
			if (addr == 0)
				return 0;
			ei->blocks[block_num] = addr;
		}
		return addr;
	}

	LOG_WARN("bmap: out of range");
	return 0;
}
