#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "kernel/stat.h"

#define LOG_MODULE "FILE"

#define NFILE 128

extern struct vfs_inode *vfs_root;
extern struct spinlock ftable_lock;
extern struct file ftable[NFILE];

struct open_path {
	struct vfs_inode *start;
	char path[PATH_MAX];
};

static int open_flags_valid(int flags)
{
	int mode = flags & O_ACCMODE;

	if (mode != O_RDONLY && mode != O_WRONLY && mode != O_RDWR)
		return -1;

	if ((flags & O_TRUNC) && mode == O_RDONLY)
		return -1;

	return 0;
}

/*
 * resolve_open_path - Choose the VFS lookup/create start point for open/openat.
 *
 * Cases:
 *   openat(AT_FDCWD, "/musl/basic/text.txt", flags)
 *     -> start at vfs_root and path is unchanged because the path is absolute.
 *
 *   openat(AT_FDCWD, "./text.txt", flags), cwd="/musl/basic"
 *     -> build "/musl/basic/text.txt", then start at vfs_root.
 *
 *   openat(dirfd, "test_openat.txt", flags)
 *     -> start at the directory inode already stored in ofile[dirfd], and path
 *        is unchanged.
 */
static int resolve_open_path(int dirfd, const char *path, struct open_path *out)
{
	if (out == 0)
		return -1;

	if (path[0] == '/') {
		out->start = vfs_root;
		strncpy(out->path, path, PATH_MAX);
		out->path[PATH_MAX - 1] = '\0';
		return 0;
	}

	struct Process *p = get_proc();
	if (dirfd == -100) {
		out->start = vfs_root;
		vfs_make_absolute_path(out->path, path);
		return 0;
	}

	if (dirfd < 0 || dirfd >= NOFILE || p->ofile[dirfd] == 0 ||
	    p->ofile[dirfd]->node == 0) {
		return -1;
	}

	out->start = p->ofile[dirfd]->node;
	strncpy(out->path, path, PATH_MAX);
	out->path[PATH_MAX - 1] = '\0';
	return 0;
}

int openat(int dirfd, const char *path, int flags)
{
	if (path == 0 || path[0] == '\0' || open_flags_valid(flags) < 0)
		return -1;

	int mode = flags & O_ACCMODE;
	struct open_path open_path; // collects the path and start node
	if (resolve_open_path(dirfd, path, &open_path) < 0)
		return -1;

	struct vfs_inode *node = vfs_lookup_at(open_path.start, open_path.path);

	if (node == 0) {
		if (!(flags & O_CREAT))
			return -1;

		// create the file
		node = vfs_create_at(open_path.start, open_path.path, VFS_FILE);
		if (node == 0)
			return -1;
	}

	if ((flags & O_TRUNC) && node->type == VFS_FILE) {
		if (node->ops == 0 || node->ops->truncate == 0) {
			vfs_iput(node);
			return -1;
		}

		vfs_ilock(node);
		int ret = node->ops->truncate(node, 0);
		vfs_iunlock(node);

		if (ret < 0) {
			vfs_iput(node);
			return -1;
		}
	}

	acquire(&ftable_lock);
	int file_id = fd_alloc();
	if (file_id == -1) {
		release(&ftable_lock);
		vfs_iput(node);
		return -1;
	}

	struct file *f = &ftable[file_id];

	f->append = (flags & O_APPEND) != 0;
	f->type = FILE_VFS_NODE;
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

int mkdirat(int dirfd, const char *path, int mode)
{
	if (path == 0 || path[0] == '\0')
		return -1;
	if (mode != 0) {
		LOG_WARN("mkdirat: mode=%d is not supported", mode);
		return -1;
	}
	struct open_path open_path; // collects the path and start node
	if (resolve_open_path(dirfd, path, &open_path) < 0)
		return -1;

	return vfs_mkdir_at(open_path.start, open_path.path, mode);
}

int unlinkat(int dirfd, const char *path, int flags)
{
	if (path == 0 || path[0] == '\0')
		return -1;

	if (flags != 0) {
		LOG_WARN("unlinkat: flags=%d is not supported", flags);
		return -1;
	}

	struct open_path open_path; // collects the path and start node
	if (resolve_open_path(dirfd, path, &open_path) < 0)
		return -1;

	return vfs_unlink_at(open_path.start, open_path.path, flags);
}

int open(const char *path, int flags)
{
	return openat(-100, path, flags);
}

int dup(int fd)
{
	if (fd < 0 || fd >= NOFILE) {
		return -1;
	}

	struct Process *proc = get_proc();
	acquire(&proc->lock);
	struct file *f = proc->ofile[fd];
	if (f == 0) {
		release(&proc->lock);
		return -1;
	}

	int newfd = -1;
	for (int i = 0; i < NOFILE; i++) {
		if (proc->ofile[i] == 0) {
			newfd = i;
			break;
		}
	}
	if (newfd == -1) {
		release(&proc->lock);
		return -1;
	}

	acquire(&ftable_lock);
	if (f->ref_count < 1)
		panic("dup");
	f->ref_count++;
	release(&ftable_lock);

	proc->ofile[newfd] = f;
	release(&proc->lock);

	return newfd;
}

int filestat(int fd, uint64 user_st_addr)
{
	struct Process *p = get_proc();

	if (fd < 0 || fd >= NOFILE || p->ofile[fd] == 0)
		return -1;

	struct file *f = p->ofile[fd];
	struct stat st;
	struct linux_stat lst = {0};

	if (f->node->ops->stat) {
		if (f->node->ops->stat(f->node, &st) < 0)
			return -1;
	} else {
		return -1;
	}

	// TODO: Fill st_mode once the kernel exposes Linux-compatible mode
	// bits.
	lst.st_nlink = st.nlink;
	lst.st_size = st.size;
	lst.st_blksize = 4096;
	lst.st_blocks = (st.size + 511) / 512;

	if (copyout(p->pagetable, (char *) user_st_addr, (uint64) &lst,
		    sizeof(lst)) < 0)
		return -1;

	return 0;
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

	enum file_type type = f->type;
	struct pipe *pi = f->pipe;
	struct vfs_inode *node = f->node;
	int writable = f->writable;

	f->type = FILE_NONE;
	f->readable = 0;
	f->writable = 0;
	f->offset = 0;
	f->f_ops = 0;
	f->node = 0;
	f->pipe = 0;

	release(&ftable_lock);

	if (type == FILE_PIPE) {
		if (pi != 0)
			pipe_close(pi, writable);
	} else if (type == FILE_VFS_NODE) {
		if (node != 0)
			vfs_iput(node);
	}
}

struct file *filedup(struct file *f)
{
	acquire(&ftable_lock);

	if (f->ref_count < 1) {
		panic("filedup");
	}

	f->ref_count++;
	release(&ftable_lock);
	return f;
}

struct file *filealloc()
{
	for (int i = 0; i < NFILE; i++) {
		acquire(&ftable_lock);
		if (ftable[i].ref_count == 0) {
			ftable[i].ref_count++;
			release(&ftable_lock);
			return ftable + i;
		}
		release(&ftable_lock);
	}

	return 0;
}
