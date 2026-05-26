#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#define NFILE 128

extern struct vfs_inode *vfs_root;
extern struct spinlock ftable_lock;
extern struct file ftable[NFILE];

static void build_cwd_path(char *dst, const char *cwd, const char *path)
{
	int i = 0;
	int j = 0;

	while (cwd[i] != '\0' && i < PATH_MAX - 1) {
		dst[i] = cwd[i];
		i++;
	}

	if (i > 1 && i < PATH_MAX - 1) {
		dst[i++] = '/';
	}

	while (path[j] != '\0' && i < PATH_MAX - 1) {
		dst[i++] = path[j++];
	}
	dst[i] = '\0';
}

static struct vfs_inode *resolve_open_node(int dirfd, const char *path)
{
	if (path[0] == '/') {
		return vfs_lookup(vfs_root, (char *) path);
	}

	struct Process *p = get_proc();
	if (dirfd == -100) {
		char fullpath[PATH_MAX];
		build_cwd_path(fullpath, p->cwd, path);
		return vfs_lookup(vfs_root, fullpath);
	}

	if (dirfd < 0 || dirfd >= NOFILE || p->ofile[dirfd] == 0 ||
	    p->ofile[dirfd]->node == 0) {
		return 0;
	}

	return vfs_lookup(p->ofile[dirfd]->node, (char *) path);
}

int openat(int dirfd, const char *path, int flags)
{

	int mode = flags & O_ACCMODE;
	// struct vfs_inode *ip;
	// if (flags & O_CREATE) {
	// 	if ((ip = create((char *) path, VFS_FILE)) == 0)
	// 		return -1;
	// } else {
	// 	if ((ip = namei((char *) path)) == 0)
	// 		return 0;
	// 	ilock(ip);
	// 	if (ip->type == VFS_DIR && mode != O_RDONLY) {
	// 		iunlockput(ip);
	// 		return -1;
	// 	}
	// }
	//
	// if (ip->type == VFS_DEV) {
	// 	iunlockput(ip);
	// 	return -1;
	// }

	struct vfs_inode *node = resolve_open_node(dirfd, path);
	if (node == 0)
		return -1;

	acquire(&ftable_lock);
	int file_id = file_alloc();
	if (file_id == -1) {
		release(&ftable_lock);
		return -1;
	}

	struct file *f = &ftable[file_id];

	f->node = node;
	f->offset = 0;
	f->ref_count = 1;
	release(&ftable_lock);

	if (mode == O_RDONLY) {
		f->readable = 1;
		f->writable = 0;
	} else if (mode == O_WRONLY) {
		f->readable = 0;
		f->writable = 1;
	} else if (mode == O_RDWR) {
		f->readable = 1;
		f->writable = 1;
	}

	return alloc_fd(get_proc(), f);
}

int open(const char *path, int flags)
{
	return openat(-100, path, flags);
}

int dup(int fd)
{
	if (fd < 0 || fd >= NFILE) {
		return -1;
	}

	struct Process *proc = get_proc();
	if (proc->ofile[fd] == 0) {
		return -1;
	}

	acquire(&ftable_lock);

	int newfd = alloc_fd(proc, proc->ofile[fd]);
	if (newfd == -1) {
		release(&ftable_lock);
		return -1;
	}
	proc->ofile[newfd]->ref_count++;
	release(&ftable_lock);

	return newfd;
}

int filestat(int fd, uint64 user_st_addr)
{
	struct Process *p = get_proc();

	if (fd < 0 || fd >= NOFILE || p->ofile[fd] == 0)
		return -1;

	struct file *f = p->ofile[fd];
	struct stat st;

	if (f->node->ops->stat) {
		if (f->node->ops->stat(f->node, &st) < 0)
			return -1;
	} else {
		return -1;
	}

	if (copyout(p->pagetable, (char *) user_st_addr, (uint64) &st,
		    sizeof(st)) < 0)
		return -1;

	return 0;
}

/**
 * create - Create a file
 *
 * Lock contract:
 * - Entry: caller holds no inode locks.
 * - During lookup: nameiparent() returns the parent directory locked; this
 *   function releases it before returning.
 * - Success: returns the existing or newly created inode with its lock held.
 * - Failure: releases every lock/reference acquired internally.
 * - Ownership: caller must release the returned inode with iunlockput().
 * */
struct vfs_inode *create(char *path, short type)
{
	struct vfs_inode *dp;
	struct vfs_inode *ip;
	char name[DIRSIZ];

	if ((dp = nameiparent(path, name)) == 0)
		return 0;

	// If the file exists, return it
	if ((ip = dirlookup(dp, name, 0)) != 0) {
		iunlockput(dp);
		ilock(ip);
		if (type == VFS_FILE &&
		    (ip->type == VFS_FILE || ip->type == VFS_DEV)) {
			LOG_DEBUG("create: file already exists");
			return ip;
		}
		iunlockput(ip);
		return 0;
	}

	// If the file does not exist, create it
	if (!(ip = ialloc(0))) {
		iunlockput(dp);
		return 0;
	}

	// If the type is VFS_DEV, set the type
	if (type == VFS_DEV && ip->type == 0) {
		ilock(ip);
		ip->type = VFS_DEV;
		iupdate(ip);
		iunlock(ip);
	}

	ilock(ip);
	ip->nlinks = 1;
	if (ip->type == 0) {
		ip->type = type;
	}
	iupdate(ip);

	if (type == VFS_DIR) { // Create . and ..
			       // . is the current directory, .. is the parent
			       // so ip->nlinks will add  1
		if (dirlink(ip, ".", ip->ino) < 0 ||
		    dirlink(ip, "..", dp->ino) < 0) {
			goto fail;
		}
	}
	if (dirlink(dp, name, ip->ino) < 0)
		goto fail;

	if (type == VFS_DIR) {
		// now that success is guaranteed:
		dp->nlinks++; // for ".."
		iupdate(dp);
	}

	iunlockput(dp);
	return ip;

fail:
	ip->nlinks = 0;
	iupdate(ip);
	iunlockput(ip);
	iunlockput(dp);
	return 0;
}

int isdirempty(struct vfs_inode *dp)
{
	int off;
	struct vfs_dirent de;

	for (off = 2 * sizeof(de); off < dp->size; off += sizeof(de)) {
		if (readi(dp, 0, (uint64) &de, off, sizeof(de)) != sizeof(de))
			panic("isdirempty: readi");
		if (de.ino != 0)
			return 0;
	}
	return 1;
}

int unlink(char *path)
{
	struct vfs_inode *ip;
	struct vfs_inode *dp;
	struct disk_dir_entry de;
	uint32 off;

	char name[DIRSIZ];
	dp = nameiparent(path, name);
	if (dp == 0) {
		LOG_WARN("unlink: parent not found");
		return -1;
	}
	ilock(dp);

	// cannot unlink "." or ".."
	if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
		goto fail;

	if ((ip = dirlookup(dp, name, &off)) == 0) {
		goto fail;
	}

	ilock(ip);
	if (ip->nlinks < 1)
		panic("unlink: nlink < 1");

	if (ip->type == VFS_DIR && !isdirempty(ip)) {
		iunlockput(ip);
		LOG_WARN("unlink: directory not empty");
		goto fail;
	}

	memset(&de, 0, sizeof(de));
	if ((writei(dp, 0, (uint64) &de, off, sizeof(de))) != sizeof(de)) {
		panic("unlink: writei failed");
	}

	if (ip->type == VFS_DIR) {
		dp->nlinks--; // Since “..” in a subdirectory refers to the
			      // parent directory, the path to the parent
			      // directory must be shortened by one.
		iupdate(dp);
	}

	iunlockput(dp);

	ip->nlinks--;
	iupdate(ip);
	iunlockput(ip);

	return 0;
fail:
	iunlockput(dp);
	return -1;
}

/**
 * fileclose - close a file
 * */
void fileclose(struct file *f)
{
	acquire(&ftable_lock);

	if (f->ref_count < 1) {
		panic("fileclose");
	}

	f->ref_count--;

	if (f->ref_count > 0) {
		release(&ftable_lock);
		return;
	}

	struct vfs_inode *node = f->node;
	int readable = f->readable;
	int writable = f->writable;

	f->node = 0;
	f->readable = 0;
	f->writable = 0;
	f->offset = 0;

	release(&ftable_lock);

	// if (node && node->default_f_ops && node->default_f_ops->close) {
	// 	node->default_f_ops->close(node);
	// }
}
