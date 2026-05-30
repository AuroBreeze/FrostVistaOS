#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "easyfs.h"

static int easyfs_read_disk_inode(uint32 ino, struct disk_inode *out);
/*
 * Lock contract:
 * - Entry: caller must hold ip->lock, and ip must be a directory.
 * - Exit: leaves ip->lock held.
 * - Return success: returns an unlocked inode with one reference from
 *   get_inode().
 * - Ownership: caller must eventually release the returned inode with
 *   put_inode(), or lock it and release it with iunlockput().
 */
struct vfs_inode *easyfs_vfs_lookup(struct vfs_inode *ip, char *name,
				    uint32 *offset)
{
	if (ip->type != VFS_DIR)
		return 0;

	struct disk_dir_entry de = {0};
	for (uint32 off = 0; off < ip->size; off += sizeof(de)) {
		if (easyfs_read_inode(ip, 0, (uint64) &de, off, sizeof(de)) !=
		    sizeof(de)) {
			panic("easyfs_read_inode read error");
		}
		if (de.inode_num == 0) {
			continue;
		}
		if (!namecmp(name, de.name)) {
			if (offset) {
				*offset = off;
			}
			// strcpy(inode->name, name);

			struct disk_inode out;
			if (easyfs_read_disk_inode(de.inode_num, &out) < 0) {
				put_inode(ip);
				return 0;
			}

			struct vfs_inode *inode =
			    easyfs_fill_vfs_inode(de.inode_num, &out, out.type);

			return inode;
		}
	}

	return 0;
}

struct vfs_inode *easyfs_get_vfs_inode(uint32 ino)
{
	struct disk_inode din;
	if (easyfs_read_disk_inode(ino, &din) < 0)
		return 0;
	return easyfs_fill_vfs_inode(ino, &din, din.type);
}

/**
 * easyfs_read_disk_inode - Read the disk inode from the disk
 *
 * Context: Take out the disk_inode data numbered ino on the disk and pass it to
 * `out`
 *
 * Return: 0
 * */
static int easyfs_read_disk_inode(uint32 ino, struct disk_inode *out)
{
	struct buf *buf;
	struct disk_inode *dip;

	uint64 blkno = ino / 64;
	buf = bread(EASYFS_DEV, blkno + 4);

	dip = (struct disk_inode *) buf->data;
	dip = &dip[ino % 64];

	*out = *dip;
	brelse(buf);

	return 0;
}
