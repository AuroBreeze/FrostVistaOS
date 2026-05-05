#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/easyfs.h"
#include "kernel/log.h"
#include "kernel/types.h"

#define NDIRECT 12
#define DIRSIZ 14

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
			temp = temp << shift;
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
	return 0;

handle_found:
	brelse(buf);
	struct buf *data_buf = bread(0, data_block);
	memset((void *) data_buf->data, 0, BSIZE);
	bwrite(data_buf);
	brelse(data_buf);
	LOG_INFO("Allocated block %d", data_block);

	return data_block;
};

/**
 * bfree - Free a data block
 *
 * Context: Mark the corresponding bit in the data bitmap as free and zero out
 * the data area
 * */
void bfree(uint32 block_num)
{
	if (block_num < 11 || block_num >= 1000) {
		panic("bfree: block number out of range");
	}

	uint32 offset = block_num - 11;
	uint32 byte_idx = offset / 8;
	uint32 bit_idx = offset % 8;

	// 3 is the Data Bitmap
	// TODO: Eliminate the Magic Number
	struct buf *buf = bread(0, 3);
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
	struct buf *data_buf = bread(0, block_num);
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

/**
 * readi - Read data from img
 *
 * Context: Read the data using the address stored in the blocks field of the
 * inode, and save it to dst
 *
 * */
uint readi(struct vfs_inode *ip, int user_dst, uint64 dst, uint32 off,
	   uint32 size)
{
	if (off > ip->size || off + size < off) {
		return 0;
	}

	// If the amount of data read exceeds the space allocated to the current
	// inode
	if (off + size > ip->size) {
		size = ip->size - off;
	}

	uint32 tot;
	uint32 m;
	for (tot = 0; tot < size; tot += m, off += m, dst += m) {
		// Get the inode address of the current offset
		uint32 addr = bmap(ip, off / BSIZE);
		// If empty, it is assigned to Gang or left idle
		if (addr == 0)
			break;

		struct buf *buf = bread(0, addr);
		m = (size - tot) > (BSIZE - (off % BSIZE))
			? BSIZE - (off % BSIZE)
			: size - tot;
		// `user_dest` is a boolean value, not an address, and is used
		// to determine whether to write to user space.
		if (user_dst) {
			struct Process *proc = get_proc();
			copyout(proc->pagetable, (void *) dst,
				(uint64) (buf->data + (off % BSIZE)), (int) m);
		} else {
			memmove((void *) dst, buf->data + (off % BSIZE), m);
		}

		brelse(buf);
	}

	return tot;
}

/**
 * skipelem: Return a pointer to the position following the next ‘/’ and copy
 * the current segment into `name`
 *
 * Return: a pointer to the position following the next ‘/’,
 * Return 0 if the path is empty
 * */
char *skipelem(char *path, char *name)
{
	while (*path == '/')
		path++;
	if (*path == '\0')
		return 0;

	char *s = path;
	while (*path != '/' && *path != '\0')
		path++;

	int len = (int) (path - s);
	if (len >= 128)
		len = 127;
	memmove(name, s, len);
	name[len] = '\0';

	while (*path == '/')
		path++;
	return path;
}

/**
 * namex - Look up and return the inode for a path name
 *
 * Context: Based on the `nameiparent` parameter, check whether the file returns
 * the inode of its parent directory
 *
 * */
struct vfs_inode *namex(char *path, int nameiparent, char *name)
{
	struct vfs_inode *ip;
	struct vfs_inode *next;

	if (*path == '/') {
		ip = get_inode(0);
	} else {
		// TODO: Search starting from the current working directory
		// ip = idup(proc->cwd);
		ip = get_inode(0);
	}

	while ((path = skipelem(path, name)) != 0) {
		if (ip->type != VFS_DIR) {
			put_inode(ip);
			LOG_WARN("namex: %s Not a directory", name);
			return 0;
		}

		if (nameiparent && *path == '\0') {
			return ip;
		}

		if ((next = dirlookup(ip, name)) == 0) {
			put_inode(ip);
			return 0;
		}

		put_inode(ip);
		ip = next;
	}

	if (nameiparent) {
		put_inode(ip);
		return 0;
	}

	return ip;
}

struct vfs_inode *namei(char *path)
{
	char name[DIRSIZ];
	return namex(path, 0, name);
}

struct vfs_inode *nameiparent(char *path, char *name)
{
	return namex(path, 1, name);
}
