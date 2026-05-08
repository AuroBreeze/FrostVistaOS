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

// xv6
// Write data to inode.
// Caller must hold ip->lock.
// If user_src==1, then src is a user virtual address;
// otherwise, src is a kernel address.
// Returns the number of bytes successfully written.
// If the return value is less than the requested n,
// there was an error of some kind.
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
		// `user_dest` is a boolean value, not an address, and is used
		// to determine whether to write to user space.
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
void itrunc(struct vfs_inode *ip)
{
	for (int i = 0; i < NDIRECT; i++) {
		struct easyfs_inode_info *ei =
		    (struct easyfs_inode_info *) ip->private_data;
		if (!ei->blocks[i]) {
			bfree(0, ei->blocks[i]);
			ei->blocks[i] = 0;
		}
	}
}

// xv6
// Write a new directory entry (name, inum) into the directory dp.
// Returns 0 on success, -1 on failure (e.g. out of disk blocks).
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
 * */
void ilock(struct vfs_inode *ip)
{
	struct buf *buf;
	struct disk_inode *dip;

	if (ip == 0 || ip->count < 1) {
		panic("ilock null inode");
	}

	acquiresleep(&ip->lock);
	// FIXME: Memory leak
	ip->private_data = (struct easyfs_inode_info *) kalloc();
	struct easyfs_inode_info *ei =
	    (struct easyfs_inode_info *) ip->private_data;

	uint64 blkno = ip->ino / 64;
	buf = bread(0, blkno + 4);

	dip = (struct disk_inode *) buf->data;
	dip = &dip[ip->ino % 64];

	ip->type = dip->type;
	ip->nlinks = dip->nlinks;
	ip->size = dip->size;
	memmove(ei->blocks, dip->blocks, sizeof(dip->blocks));

	brelse(buf);
	ei->vaild = 1;
	if (ip->type == 0) {
		// panic("ilock: no type");
		LOG_TRACE("ilock: no type");
	}
}

// xv6
// Unlock the given inode.
void iunlock(struct vfs_inode *ip)
{
	if (ip == 0 || !holdingsleep(&ip->lock) || ip->count < 1)
		panic("iunlock");

	// PERF: Handle private in a better way
	// FIXME: Memory leak
	kfree(ip->private_data);
	releasesleep(&ip->lock);
}

// xv6
// Common idiom: unlock, then put.
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
 * Return: the ip holding the sleeplock
 * */
static struct vfs_inode *namex(char *path, int nameiparent, char *name)
{
	struct vfs_inode *ip;
	struct vfs_inode *next;

	if (*path == '/') { // NOLINT(bugprone-branch-clone)
		ip = get_inode(0);
	} else {
		// TODO: Search starting from the current working directory
		// ip = idup(proc->cwd);
		ip = get_inode(0);
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
		*ip = *next;
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

int namecmp(const char *s, const char *t)
{
	return strncmp(s, t, DIRSIZ);
}
