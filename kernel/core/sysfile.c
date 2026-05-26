#include "kernel/sysfile.h"
#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "kernel/spinlock.h"
#include "kernel/types.h"
#include "kernel/syscall.h"
#define NFILE 128

extern struct spinlock ftable_lock;
extern struct file ftable[NFILE];
extern struct vfs_inode *vfs_root;

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
	if (fd < 0 || fd >= NFILE || proc->ofile[fd] == 0) {
		return -1;
	}

	acquire(&proc->lock);
	struct file *file = proc->ofile[fd];
	proc->ofile[fd] = 0;
	release(&proc->lock);

	acquire(&ftable_lock);
	file->ref_count--;
	if (file->ref_count == 0)
		file->node = 0;
	// file->node->ops->close(file->node);
	release(&ftable_lock);

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
	struct Process *proc = get_proc();
	int newfd = dup(oldfd);
	LOG_TRACE("sys_dup: newfd=%d, newfile_ref=%d, oldfile_ref=%d", newfd,
		  proc->ofile[newfd]->ref_count, proc->ofile[oldfd]->ref_count);
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

	char path[PATH_MAX];
	if (argstr(ARG1, path, PATH_MAX) < 0)
		return -1;

	return open(path, flags);
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
	LOG_ERROR("sys_getcwd: not implemented");
	return -1;
}

uint64 sys_chdir()
{
	LOG_ERROR("sys_chdir: not implemented");
	return -1;
}

uint64 sys_mkdirat()
{
	LOG_ERROR("sys_mkdirat: not implemented");
	return -1;
}

uint64 sys_unlinkat()
{
	LOG_ERROR("sys_unlinkat: not implemented");
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
	LOG_ERROR("sys_lseek: not implemented");
	return -1;
}

uint64 sys_newfstatat()
{
	LOG_ERROR("sys_newfstatat: not implemented");
	return -1;
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
	LOG_ERROR("sys_dup3: not implemented");
	return -1;
}
