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
	uint32 max_data_blocks = TOTAL_BLOCKS - DATA_BLOCK;
	for (uint32 bit = 0; bit < max_data_blocks; bit++) {
		uint32 byte_idx = bit / 8;
		uint32 bit_idx = bit % 8;
		// All slots are currently filled
		if (buf->data[byte_idx] == 0xFF)
			continue;

		uint8 mask = 1 << bit_idx;
		if (!(buf->data[byte_idx] & mask)) {
			buf->data[byte_idx] |= mask;
			data_block = bit + DATA_BLOCK;
			goto handle_found;
		}
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
	if (block_num < DATA_BLOCK || block_num >= TOTAL_BLOCKS) {
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
 * @block_num: Logical block number inside the file.
 *
 * Planned indirect-block mapping:
 * - [0, NDIRECT): direct data blocks.
 * - next NINDIRECT blocks: blocks[10] points to one indirect block that stores
 *   data block numbers.
 * - remaining NDINDIRECT blocks: blocks[11] points to a double-indirect block
 *   that stores second-level indirect block numbers; each second-level block
 *   stores data block numbers.
 *
 * Lock contract:
 * - Entry: caller should hold ip->lock to stabilize and, if needed, update
 *   the inode block mapping.
 * - Exit: leaves ip->lock state unchanged.
 * - Side effect: may allocate a new data block and store it in ip private
 *   metadata when the mapping is empty.
 * */
uint bmap(struct vfs_inode *ip, uint32 block_num)
{

	if (block_num < NDIRECT) {
		uint32 addr;
		struct easyfs_inode_info *ei =
		    (struct easyfs_inode_info *) ip->private_data;

		if ((addr = ei->blocks[block_num]) == 0) {
			addr = balloc(EASYFS_DEV);
			if (addr == 0)
				return 0;
			ei->blocks[block_num] = addr;
		}
		return addr;
	} else if (block_num < NDIRECT + NINDIRECT) {
		uint32 idx = block_num - NDIRECT;

		struct easyfs_inode_info *ei =
		    (struct easyfs_inode_info *) ip->private_data;

		uint32 indirect_addr = ei->blocks[SINDIRECT_INDEX];
		if (indirect_addr == 0) {
			indirect_addr = balloc(EASYFS_DEV);
			if (indirect_addr == 0)
				return 0;
			ei->blocks[SINDIRECT_INDEX] = indirect_addr;
		}

		struct buf *indirect = bread(EASYFS_DEV, indirect_addr);
		uint32 *addrs = (uint32 *) indirect->data;

		uint32 addr = addrs[idx];
		if (addr == 0) {
			addr = balloc(EASYFS_DEV);
			if (addr == 0) {
				brelse(indirect);
				return 0;
			}
			addrs[idx] = addr;
			bwrite(indirect);
		}

		brelse(indirect);
		return addr;
	} else if (block_num < NDIRECT + NINDIRECT + NDINDIRECT) {
		uint32 idx = block_num - NDIRECT - NINDIRECT;

		struct easyfs_inode_info *ei =
		    (struct easyfs_inode_info *) ip->private_data;
		uint32 double_indirect = ei->blocks[DINDIRECT_INDEX];

		if (double_indirect == 0) {
			double_indirect = balloc(EASYFS_DEV);
			if (double_indirect == 0) {
				return 0;
			}
			ei->blocks[DINDIRECT_INDEX] = double_indirect;
		}

		struct buf *double_indirect_buf =
		    bread(EASYFS_DEV, double_indirect);

		uint32 *addrs = (uint32 *) double_indirect_buf->data;

		uint32 double_indirect_offset = idx / NINDIRECT;
		uint32 double_indirect_idx = idx % NINDIRECT;

		if (addrs[double_indirect_offset] == 0) {
			addrs[double_indirect_offset] = balloc(EASYFS_DEV);
			if (addrs[double_indirect_offset] == 0) {
				brelse(double_indirect_buf);
				return 0;
			}
			bwrite(double_indirect_buf);
		}

		uint32 indirect_addr = addrs[double_indirect_offset];
		struct buf *indirect_buf = bread(EASYFS_DEV, indirect_addr);
		uint32 *indirect_addrs = (uint32 *) indirect_buf->data;

		if (indirect_addrs[double_indirect_idx] == 0) {
			indirect_addrs[double_indirect_idx] =
			    balloc(EASYFS_DEV);
			if (indirect_addrs[double_indirect_idx] == 0) {
				brelse(indirect_buf);
				brelse(double_indirect_buf);
				return 0;
			}
			bwrite(indirect_buf);
		}

		uint32 addr = indirect_addrs[double_indirect_idx];
		brelse(indirect_buf);
		brelse(double_indirect_buf);
		return addr;
	}

	LOG_WARN("bmap: out of range");
	return 0;
}
