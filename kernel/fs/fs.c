#include "kernel/fs.h"
#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/easyfs.h"
#include "kernel/log.h"
#include "kernel/types.h"

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
		// `user_dst` is a boolean value, not an address, and is used
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
int writei(struct vfs_inode *ip, int user_src, uint64 src, uint32 off,
	   uint32 size)
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
void itrunc(struct vfs_inode *ip)
{
	for (int i = 0; i < NDIRECT; i++) {
		struct easyfs_inode_info *ei =
		    (struct easyfs_inode_info *) ip->private_data;
		if (ei->blocks[i]) {
			bfree(0, ei->blocks[i]);
			ei->blocks[i] = 0;
		}
	}
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

	if ((ip = dirlookup(dp, name, 0)) != 0) {
		put_inode(ip);
		LOG_WARN("dirlink: %s already exists", name);
		return -1;
	}
	// Look for an empty dirent.
	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (readi(dp, 0, (uint64) &de, off, sizeof(de)) != sizeof(de))
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
	if (writei(dp, 0, (uint64) &de, off, sizeof(de)) != sizeof(de))
		return -1;

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
void ilock(struct vfs_inode *ip)
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
	ei->valid = 1;
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
void iunlock(struct vfs_inode *ip)
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
void iunlockput(struct vfs_inode *ip)
{
	iunlock(ip);
	put_inode(ip);
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
	if (len >= PATH_MAX)
		len = PATH_MAX - 1;
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
 * Return: the ip holding the sleeplock
 *
 * Lock contract:
 * - Entry: caller holds no inode locks.
 * - During traversal: locks each directory inode only while inspecting it.
 * - nameiparent == 0 success: returns a referenced inode, unlocked.
 * - nameiparent == 1 success: returns the referenced parent inode with its
 *   lock held.
 * - Failure: releases locks/references acquired during traversal.
 * - Ownership: caller must release the returned inode according to its lock
 *   state.
 * */
static struct vfs_inode *namex(char *path, int nameiparent, char *name)
{
	struct vfs_inode *ip;
	struct vfs_inode *next;

	if (*path == '/') { // NOLINT(bugprone-branch-clone)
		ip = get_inode(EASYFS_DEV, SUPER_INUM);
	} else {
		// TODO: Search starting from the current working directory
		// ip = idup(proc->cwd);
		ip = get_inode(EASYFS_DEV, SUPER_INUM);
	}

	while ((path = skipelem(path, name)) != 0) {
		ilock(ip);
		if (ip->type != VFS_DIR) {
			iunlockput(ip);
			LOG_WARN("namex: %s Not a directory", name);
			return 0;
		}

		if (nameiparent && *path == '\0') {
			return ip;
		}

		if ((next = dirlookup(ip, name, 0)) == 0) {
			iunlockput(ip);
			return 0;
		}

		iunlockput(ip);
		ip = next;
	}

	if (nameiparent) {
		put_inode(ip);
		return 0;
	}

	return ip;
}

/**
 * namei - Look up and return the inode for a path name
 *
 * Lock contract:
 * - Entry: caller holds no inode locks.
 * - Success: returns a referenced inode, unlocked.
 * - Failure: releases all locks/references acquired internally.
 * - Ownership: caller must eventually call put_inode(), or lock and release
 *   the inode with iunlockput().
 * */
struct vfs_inode *namei(char *path)
{
	char name[DIRSIZ];
	return namex(path, 0, name);
}

/**
 * nameiparent - Look up and return the inode's parent for a path namee
 *
 * Lock contract:
 * - Entry: caller holds no inode locks.
 * - Success: returns a referenced parent inode with its lock held, and stores
 *   the final path element in name.
 * - Failure: releases all locks/references acquired internally.
 * - Ownership: caller must release the returned parent with iunlockput().
 * */
struct vfs_inode *nameiparent(char *path, char *name)
{
	return namex(path, 1, name);
}

int namecmp(const char *s, const char *t)
{
	return strncmp(s, t, DIRSIZ);
}
