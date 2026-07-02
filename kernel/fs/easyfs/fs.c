#include "asm/defs.h"
#include "kernel/defs.h"
#include "kernel/types.h"
#include "kernel/fs.h"
#include "easyfs.h"
#include "kernel/bcache.h"
#include "core/proc.h"
#include "kernel/log.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

/**
 * readi - Read data from img
 *
 * Context: Read the data using the address stored in the blocks field of the
 * inode, and save it to dst
 *
 * Lock contract:
 * - Entry: caller should hold ip->lock to stabilize inode size and block
 *   metadata while reading.
 * - Exit: does not acquire or release ip->lock.
 * - Ownership: does not change the inode reference count.
 *
 * */
int easyfs_read_inode(struct vfs_inode *ip, int user_dst, uint64 dst,
		      uint32 off, uint32 size)
{
	if (off > ip->size || off + size < off) {
		return -1;
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
		// `user_dst` is a boolean value, not an address, and is used
		// to determine whether to write to user space.
		if (user_dst) {
			struct Process *proc = get_proc();
			if (copyout(proc->pagetable, (void *) dst,
				    (uint64) (buf->data + (off % BSIZE)),
				    (int) m) < 0) {
				brelse(buf);
				return -1;
			}
		} else {
			memmove((void *) dst, buf->data + (off % BSIZE), m);
		}

		brelse(buf);
	}

	return tot;
}

// xv6
// Write data to inode.
// Caller must hold ip->lock.
// If user_src==1, then src is a user virtual address;
// otherwise, src is a kernel address.
// Returns the number of bytes successfully written.
// If the return value is less than the requested n,
// there was an error of some kind.
/*
 * Lock contract:
 * - Entry: caller must hold ip->lock.
 * - Exit: leaves ip->lock held.
 * - Ownership: does not change the inode reference count.
 */
int easyfs_write_inode(struct vfs_inode *ip, int user_src, uint64 src,
		       uint32 off, uint32 size)
{
	uint32 tot;
	uint32 m;
	struct buf *bp;

	if (off + size < off)
		return -1;
	if (off + size > MAXFILE * BSIZE)
		return -1;

	for (tot = 0; tot < size; tot += m, off += m, src += m) {
		uint addr = bmap(ip, off / BSIZE);
		if (addr == 0)
			break;
		bp = bread(0, addr);
		m = min(size - tot, BSIZE - (off % BSIZE));
		// `user_src` is a boolean value, not an address, and is used
		// to determine whether to copy from user space.
		if (user_src) {
			struct Process *proc = get_proc();
			if (copyin(proc->pagetable,
				   (char *) bp->data + (off % BSIZE), src,
				   m) < 0) {
				brelse(bp);
				return -1;
			}
		} else {
			memmove((char *) bp->data + (off % BSIZE), (void *) src,
				m);
		}

		bwrite(bp);
		brelse(bp);
	}

	if (off > ip->size)
		ip->size = off;

	// write the i-node back to disk even if the size didn't change
	// because the loop above might have called bmap() and added a new
	// block to ip->addrs[].
	iupdate(ip);

	return tot;
}

// xv6
// only free direct blocks
/*
 * Lock contract:
 * - Entry: caller must hold ip->lock.
 * - Exit: leaves ip->lock held after freeing direct data blocks from the
 *   inode metadata.
 * - Ownership: does not change the inode reference count.
 */
int easyfs_itrunc(struct vfs_inode *ip, uint64 size)
{
	if (size != 0)
		return -1;

	struct easyfs_inode_info *ei =
	    (struct easyfs_inode_info *) ip->private_data;

	for (int i = 0; i < NDIRECT; i++) {
		if (ei->blocks[i]) {
			bfree(EASYFS_DEV, ei->blocks[i]);
			ei->blocks[i] = 0;
		}
	}

	if (ei->blocks[SINDIRECT_INDEX] != 0) {
		struct buf *buf =
		    bread(EASYFS_DEV, ei->blocks[SINDIRECT_INDEX]);
		uint32 *addr = (uint32 *) buf->data;

		for (int i = 0; i < NINDIRECT; i++) {
			if (addr[i] != 0) {
				bfree(EASYFS_DEV, addr[i]);
				addr[i] = 0;
			}
		}

		brelse(buf);

		bfree(EASYFS_DEV, ei->blocks[SINDIRECT_INDEX]);
		ei->blocks[SINDIRECT_INDEX] = 0;
	}

	if (ei->blocks[DINDIRECT_INDEX] != 0) {
		struct buf *indirect_buf =
		    bread(EASYFS_DEV, ei->blocks[DINDIRECT_INDEX]);
		uint32 *indirect_addr = (uint32 *) indirect_buf->data;

		for (int i = 0; i < NINDIRECT; i++) {
			if (indirect_addr[i] != 0) {
				struct buf *double_indirect_buf =
				    bread(EASYFS_DEV, indirect_addr[i]);
				uint32 *double_indirect_addr =
				    (uint32 *) double_indirect_buf->data;
				for (int j = 0; j < NINDIRECT; j++) {
					if (double_indirect_addr[j] != 0) {
						bfree(EASYFS_DEV,
						      double_indirect_addr[j]);
						double_indirect_addr[j] = 0;
					}
				}
				brelse(double_indirect_buf);

				bfree(EASYFS_DEV, indirect_addr[i]);
				indirect_addr[i] = 0;
			}
		}
		brelse(indirect_buf);

		bfree(EASYFS_DEV, ei->blocks[DINDIRECT_INDEX]);
		ei->blocks[DINDIRECT_INDEX] = 0;
	}

	ip->size = size;
	iupdate(ip);
	return 0;
}

// xv6
/**
 * ilock - Lock inode and read node->ino to node->private_data
 *
 * Context: Allocate space for `private`, instantiate it, obtain the
 * corresponding block region, convert it to a disk_inode, and copy the
 * disk_inode data to the inode.
 *
 * Lock contract:
 * - Entry: caller must hold a live inode reference (ip->count > 0).
 * - Entry: caller must not already hold ip->lock (sleeplock).
 * - Exit success: returns with ip->lock held and the on-disk inode fields
 *   loaded into memory.
 * - Ownership: caller must later release the lock with iunlock() or
 *   iunlockput().
 *
 * */
void easyfs_ilock(struct vfs_inode *ip)
{
	struct buf *buf;
	struct disk_inode *dip;

	if (ip == 0 || ip->count < 1) {
		panic("ilock null inode");
	}

	acquiresleep(&ip->lock);

	if (ip->private_data == 0) {
		LOG_WARN("ilock: no private data");
	}
	struct easyfs_inode_info *ei = ip->private_data;

	uint64 blkno = ip->ino / 64;
	buf = bread(0, blkno + 4);

	dip = (struct disk_inode *) buf->data;
	dip = &dip[ip->ino % 64];

	ip->type = dip->type;
	ip->nlinks = dip->nlinks;
	ip->size = dip->size;
	memmove(ei->blocks, dip->blocks, sizeof(dip->blocks));

	brelse(buf);
	if (ip->type == 0) {
		LOG_TRACE("ilock: no type");
	}
}

// xv6
// Unlock the given inode.
/*
 * Lock contract:
 * - Entry: caller must hold ip->lock.
 * - Exit: releases ip->lock.
 * - Ownership: does not drop the inode reference count.
 */
void easyfs_iunlock(struct vfs_inode *ip)
{
	if (ip == 0 || !holdingsleep(&ip->lock) || ip->count < 1)
		panic("iunlock");

	// PERF: Handle private in a better way
	releasesleep(&ip->lock);
}

// xv6
// Common idiom: unlock, then put.
/*
 * Lock contract:
 * - Entry: caller must hold ip->lock and own one inode reference.
 * - Exit: releases ip->lock and drops one inode reference with put_inode().
 * - Ownership: caller must not use ip after this call unless it owns another
 *   reference.
 */
void easyfs_iunlockput(struct vfs_inode *ip)
{
	easyfs_iunlock(ip);
	put_inode(ip, 1);
}

// xv6
// Write a new directory entry (name, inum) into the directory dp.
// Returns 0 on success, -1 on failure (e.g. out of disk blocks).
/*
 * Lock contract:
 * - Entry: caller must hold dp->lock, and dp must be a directory.
 * - Exit: leaves dp->lock held.
 * - Ownership: does not change the parent directory reference count.
 */
int dirlink(struct vfs_inode *dp, char *name, uint inum)
{
	struct disk_dir_entry de;
	struct vfs_inode *ip;
	uint32 off;

	if ((ip = easyfs_vfs_lookup(dp, name, 0)) != 0) {
		put_inode(ip, 1);
		LOG_WARN("dirlink: %s already exists", name);
		return -1;
	}
	// Look for an empty dirent.
	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (easyfs_read_inode(dp, 0, (uint64) &de, off, sizeof(de)) !=
		    sizeof(de))
			panic("dirlink read");
		if (de.inode_num == 0)
			// HACK: Could it be that the lack of finding an exit
			// leads to overwriting?
			break;
	}

	// Clear the dirent.
	memset(&de, 0, sizeof(de));

	strncpy(de.name, name, DIRSIZ);
	de.inode_num = inum;
	if (easyfs_write_inode(dp, 0, (uint64) &de, off, sizeof(de)) !=
	    sizeof(de))
		return -1;

	return 0;
}
