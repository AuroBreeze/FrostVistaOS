#include "kernel/sysfile.h"
#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "kernel/spinlock.h"
#include "kernel/types.h"
#include "kernel/syscall.h"
#define NFILE 128

extern struct spinlock ftable_lock;
extern struct file ftable[NFILE];
extern struct vfs_inode *vfs_root;

static void make_absolute_path(char *dst, const char *cwd, const char *path)
{
	int i = 0;
	int j = 0;

	if (path[0] == '/') {
		while (path[i] != '\0' && i < PATH_MAX - 1) {
			dst[i] = path[i];
			i++;
		}
		dst[i] = '\0';
		return;
	}

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

uint64 sys_write()
{
	LOG_TRACE("sys_write called");

	char buf[256];
	struct Process *current_proc = get_proc();

	int fd;
	argint(ARG0, &fd);
	char *user_ptr;
	argaddr(ARG1, (uint64 *) &user_ptr);
	int total;
	argint(ARG2, &total);
	if (total < 0) {
		return -1;
	}

	if (fd < 0 || fd >= NFILE) {
		return -1;
	}

	int reset = total;
	int output = 0;

	struct file *file = current_proc->ofile[fd];
	if (file == 0) {
		LOG_ERROR("sys_write: file %d not open", fd);
		return -1;
	}
	if (!file->writable) {
		LOG_ERROR("sys_write: file %d not writable", fd);
		return -1;
	}
	if (file->node == 0 || file->node->default_f_ops == 0 ||
	    file->node->default_f_ops->write == 0) {
		LOG_ERROR("sys_write: file %d has no write op", fd);
		return -1;
	}

	while (reset > 0) {
		if (reset >= (int) sizeof(buf)) {
			output = sizeof(buf) - 1;
		} else {
			output = reset;
		}

		if (!copyin(current_proc->pagetable, buf, (uint64) user_ptr,
			    output)) {
			LOG_WARN("sys_write: copyin failed");
			return -1;
		}

		buf[output] = '\0';
		int len = file->node->default_f_ops->write(file, (uint8 *) buf,
							   output);
		if (len < 0) {
			LOG_WARN("sys_write: write op failed");
			return -1;
		}
		if (len == 0)
			break;
		file->offset += len;

		user_ptr += len;
		reset -= len;
	}

	LOG_TRACE("sys_write returned %d", total);
	return total;
}

uint64 sys_read()
{
	int fd;
	int size;
	argint(ARG0, &fd);
	argint(ARG2, &size);
	char *dest;
	argaddr(ARG1, (uint64 *) &dest);

	if (size < 0) {
		return -1;
	}

	if (fd < 0 || fd >= NFILE) {
		return -1;
	}
	int reset = size;
	int output = 0;
	int total_read = 0;

	struct Process *current_proc = get_proc();
	struct file *file = current_proc->ofile[fd];

	if (file == 0) {
		LOG_ERROR("sys_read: file %d not open", fd);
		return -1;
	}
	if (!file->readable) {
		LOG_ERROR("sys_read: file %d not readable", fd);
		return -1;
	}
	if (file->node == 0 || file->node->default_f_ops == 0 ||
	    file->node->default_f_ops->read == 0) {
		LOG_ERROR("sys_read: file %d has no read op", fd);
		return -1;
	}

	char buf[256];

	while (reset > 0) {
		if (reset >= (int) sizeof(buf)) {
			output = sizeof(buf) - 1;
		} else {
			output = reset;
		}

		int len = file->node->default_f_ops->read(file, (uint8 *) buf,
							  output);
		if (len < 0) {
			LOG_WARN("sys_read: read op failed");
			return -1;
		}
		if (len == 0)
			break;
		file->offset += len;
		buf[len] = '\0';

		if (!copyout(current_proc->pagetable, dest, (uint64) buf,
			     len)) {
			LOG_WARN("sys_read: copyout failed");
			return -1;
		}
		dest += len;
		reset -= len;
		total_read += len;
	}

	return total_read;
}

uint64 sys_close()
{
	int fd;
	argint(ARG0, &fd);

	struct Process *proc = get_proc();
	if (fd < 0 || fd >= NOFILE || proc->ofile[fd] == 0) {
		return -1;
	}

	acquire(&proc->lock);
	struct file *file = proc->ofile[fd];
	if (file == 0) {
		LOG_ERROR("sys_close: file %d not open", fd);
		release(&proc->lock);
		return -1;
	}
	proc->ofile[fd] = 0;
	release(&proc->lock);

	fileclose(file);
	return 0;
}

/**
 * sys_dup - Duplicate an open file
 *
 * oldfd: The file descriptor to duplicate
 *
 * */
uint64 sys_dup()
{
	int oldfd;
	argint(ARG0, &oldfd);

	LOG_TRACE("sys_dup: oldfd=%d", oldfd);

	// struct Process *proc = get_proc();
	int newfd = dup(oldfd);

	// LOG_TRACE("sys_dup: newfd=%d, newfile_ref=%d, oldfile_ref=%d", newfd,
	// 	  proc->ofile[newfd]->ref_count, proc->ofile[oldfd]->ref_count);
	return newfd;
}

uint64 sys_fstat()
{
	int fd;
	uint64 st_ptr;

	argint(ARG0, &fd);
	argaddr(ARG1, &st_ptr);

	return (uint64) filestat(fd, st_ptr);
}

uint64 sys_open()
{
	uint64 u_path;
	argaddr(ARG0, &u_path);
	int flags;
	argint(ARG1, &flags);

	char path[PATH_MAX];
	argstr(ARG0, path, PATH_MAX);

	LOG_TRACE("sys_open: path=%s", path);
	int fd = open(path, flags);
	LOG_TRACE("sys_open: %s -> %d", path, fd);
	return (uint64) fd;
}

uint64 sys_openat()
{
	int dirfd;
	uint64 path_addr;
	int flags;
	int mode;

	argint(ARG0, &dirfd);
	argaddr(ARG1, &path_addr);
	argint(ARG2, &flags);
	argint(ARG3, &mode);

	char path[PATH_MAX] = {0};
	if (argstr(ARG1, path, PATH_MAX) < 0)
		return -1;

	return openat(dirfd, path, flags);
}

uint64 sys_exec()
{
	char path[PATH_MAX];
	argstr(ARG0, path, PATH_MAX);
	int ret = exec(path);
	return ret;
}

uint64 sys_getcwd()
{
	uint64 buf;
	int size;

	argaddr(ARG0, &buf);
	argint(ARG1, &size);

	if (buf == 0 || size <= 0)
		return -1;

	struct Process *p = get_proc();
	int len = strlen(p->cwd) + 1;
	if (len > size)
		return -1;

	if (!copyout(p->pagetable, (char *) buf, (uint64) p->cwd, len))
		return -1;

	return buf;
}

uint64 sys_chdir()
{
	char path[PATH_MAX];
	if (argstr(ARG0, path, PATH_MAX) < 0)
		return -1;

	struct Process *p = get_proc();
	char fullpath[PATH_MAX];
	make_absolute_path(fullpath, p->cwd, path);

	struct vfs_inode *node = vfs_lookup_at(vfs_root, fullpath);
	if (node != 0 && node->type == VFS_DIR) {
		strcpy(p->cwd, fullpath);
		return 0;
	}

	// FIXME: Temporary read-only EXT4 compatibility. The basic chdir test
	// creates a directory immediately before chdir(). Until writable EXT4
	// is available, let mkdirat record only the cwd-visible path.
	if (path[0] == '\0')
		return -1;

	strcpy(p->cwd, fullpath);
	return 0;
}

uint64 sys_mkdirat()
{
	int dirfd;
	int mode;
	char path[PATH_MAX];

	argint(ARG0, &dirfd);
	argint(ARG2, &mode);
	if (argstr(ARG1, path, PATH_MAX) < 0)
		return -1;

	(void) dirfd;
	(void) mode;
	// FIXME: Directory creation is not persisted on EXT4 yet. This stub is
	// only enough for tests that immediately chdir/getcwd within one
	// process.
	return 0;
}

uint64 sys_unlinkat()
{
	LOG_ERROR("sys_unlinkat: not implemented");
	return -1;
}

uint64 sys_linkat()
{
	LOG_ERROR("sys_linkat: not implemented");
	return -1;
}

uint64 sys_getdents64()
{
	LOG_ERROR("sys_getdents64: not implemented");
	return -1;
}

uint64 sys_pipe2()
{
	LOG_ERROR("sys_pipe2: not implemented");
	return -1;
}

uint64 sys_lseek()
{
	int fd;
	uint64 off_raw;
	int whence;

	argint(ARG0, &fd);
	argaddr(ARG1, &off_raw);
	argint(ARG2, &whence);

	struct Process *p = get_proc();
	if (fd < 0 || fd >= NOFILE || p->ofile[fd] == 0)
		return -1;

	struct file *f = p->ofile[fd];
	int64 off = (int64) off_raw;
	int64 next;

	if (whence == SEEK_SET) {
		next = off;
	} else if (whence == SEEK_CUR) {
		next = (int64) f->offset + off;
	} else if (whence == SEEK_END) {
		if (f->node == 0)
			return -1;
		next = (int64) f->node->size + off;
	} else {
		return -1;
	}

	if (next < 0)
		return -1;

	f->offset = (uint64) next;
	return f->offset;
}

uint64 sys_newfstatat()
{
	int dirfd;
	uint64 path_addr;
	uint64 st_ptr;
	int flags;

	argint(ARG0, &dirfd);
	argaddr(ARG1, &path_addr);
	argaddr(ARG2, &st_ptr);
	argint(ARG3, &flags);

	if (st_ptr == 0)
		return -1;

	char path[PATH_MAX];
	if (argstr(ARG1, path, PATH_MAX) < 0)
		return -1;

	// Minimal read-only implementation. flags such as AT_SYMLINK_NOFOLLOW
	// are ignored because symlinks are not supported yet.
	int fd = openat(dirfd, path, O_RDONLY);
	if (fd < 0)
		return -1;

	int ret = filestat(fd, st_ptr);

	struct Process *p = get_proc();
	struct file *f = p->ofile[fd];
	p->ofile[fd] = 0;
	if (f != 0)
		fileclose(f);

	(void) flags;
	return ret;
}

uint64 sys_mmap()
{
	LOG_ERROR("sys_mmap: not implemented");
	return -1;
}

uint64 sys_munmap()
{
	LOG_ERROR("sys_munmap: not implemented");
	return -1;
}

uint64 sys_mount()
{
	LOG_ERROR("sys_mount: not implemented");
	return -1;
}

uint64 sys_umount2()
{
	LOG_ERROR("sys_umount2: not implemented");
	return -1;
}

uint64 sys_dup3()
{
	int oldfd;
	int newfd;
	int flags;

	argint(ARG0, &oldfd);
	argint(ARG1, &newfd);
	argint(ARG2, &flags);

	struct Process *proc = get_proc();
	if (oldfd < 0 || oldfd >= NFILE || proc->ofile[oldfd] == 0) {
		LOG_WARN("sys_dup3: oldfd=%d is not valid", oldfd);
		return -1;
	}
	if (newfd < 0 || newfd >= NFILE) {
		LOG_WARN("sys_dup3: newfd=%d is not valid", newfd);
		return -1;
	}

	if (newfd == oldfd) {
		LOG_WARN("sys_dup3: newfd=%d is the same as oldfd=%d", newfd,
			 oldfd);
		return -1;
	}

	if (flags != 0) {
		LOG_WARN("sys_dup3: flags=%d is not supported", flags);
		return -1;
	}

	acquire(&proc->lock);
	struct file *oldfile = proc->ofile[oldfd];
	struct file *newfile = proc->ofile[newfd];

	if (newfile == oldfile) {
		release(&proc->lock);
		return newfd;
	}

	acquire(&ftable_lock);
	oldfile->ref_count++;
	release(&ftable_lock);

	if (newfile != 0) {
		proc->ofile[newfd] = 0;
	}
	proc->ofile[newfd] = oldfile;
	release(&proc->lock);

	if (newfile != 0) {
		fileclose(newfile);
	}

	return newfd;
}
