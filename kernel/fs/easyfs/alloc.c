#include "kernel/bcache.h"
#include "kernel/types.h"
#include "easyfs.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/fs.h"
/**
 * balloc - Allocate a free data block
 *
 * Context: Search for free bits in the data bitmap on page 3 and allocate the
 * corresponding space in the data area.
 *
 * Lock contract:
 * - Entry: caller holds no buffer locks for the data bitmap or allocated block.
 * - Exit: releases all buffer locks acquired internally.
 * - Success: returns an allocated, zeroed data block number.
 * - Failure: returns 0.
 *
 * Return: Returns the block address of the corresponding data area found
 * */
uint32 balloc(uint32 dev)
{
	struct buf *buf;
	uint32 data_block;

	buf = bread(dev, DABLK_BMIP);
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

				data_block = (i * 8) + shift + DATA_BLOCK;
				goto handle_found;
			}
		}
		LOG_WARN("balloc: out of space");
	}
	LOG_WARN("balloc: No available bits found");
	brelse(buf);
	return 0;

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
 *
 * Lock contract:
 * - Entry: caller holds no buffer locks for the data bitmap or target block.
 * - Exit: releases all buffer locks acquired internally.
 * - Ownership: caller must ensure no live inode metadata still references
 *   block_num.
 * */
void bfree(uint32 dev, uint32 block_num)
{
	if (block_num < DATA_BLOCK || block_num >= 1000) {
		panic("bfree: block number out of range");
	}

	uint32 offset = block_num - DATA_BLOCK;
	uint32 byte_idx = offset / 8;
	uint32 bit_idx = offset % 8;

	struct buf *buf = bread(dev, DABLK_BMIP);
	int mask = (1 << bit_idx);

	// A clear bit means the block was already free.
	if (!(buf->data[byte_idx] & mask)) {
		panic("bfree: block already free");
	}

	buf->data[byte_idx] &= ~mask;
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
 *
 * Lock contract:
 * - Entry: caller should hold ip->lock to stabilize and, if needed, update
 *   the inode block mapping.
 * - Exit: leaves ip->lock state unchanged.
 * - Side effect: may allocate a new data block and store it in ip private
 *   metadata when the mapping is empty.
 * */
// TODO: For now, we are using only 12 blocks and not using indirect addresses.
uint bmap(struct vfs_inode *ip, uint32 block_num)
{
	if (block_num < NDIRECT) {
		uint32 addr;
		struct easyfs_inode_info *ei =
		    (struct easyfs_inode_info *) ip->private_data;
		if ((addr = ei->blocks[block_num]) == 0) {
			addr = balloc(0);
			if (addr == 0)
				return 0;
			ei->blocks[block_num] = addr;
		}
		return addr;
	}

	LOG_WARN("bmap: out of range");
	return 0;
}
