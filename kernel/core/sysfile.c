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

#define LOG_MODULE "SYSF"

#define NFILE 128
#define MAX_EXEC_ARGS 16

extern struct spinlock ftable_lock;
extern struct file ftable[NFILE];
extern struct vfs_inode *vfs_root;

static int path_elem_eq(const char *s, int len, const char *t)
{
	int i = 0;

	while (i < len && t[i] != '\0') {
		if (s[i] != t[i])
			return 0;
		i++;
	}

	return i == len && t[i] == '\0';
}

static void append_path_elem(char *out, int *out_len, const char *elem, int len)
{
	if (*out_len > 1 && *out_len < PATH_MAX - 1)
		out[(*out_len)++] = '/';

	for (int i = 0; i < len && *out_len < PATH_MAX - 1; i++)
		out[(*out_len)++] = elem[i];
	out[*out_len] = '\0';
}

static void pop_path_elem(char *out, int *out_len)
{
	// only '/'
	if (*out_len <= 1) {
		out[0] = '/';
		out[1] = '\0';
		*out_len = 1;
		return;
	}

	while (*out_len > 1 && out[*out_len - 1] != '/')
		(*out_len)--;

	// delete '/'
	if (*out_len > 1)
		(*out_len)--;
	out[*out_len] = '\0';
}

static void normalize_absolute_path(char *dst, const char *path)
{
	char out[PATH_MAX] = {0};
	int out_len = 1;
	int i = 0;

	out[0] = '/';
	while (path[i] != '\0') {
		while (path[i] == '/')
			i++;
		if (path[i] == '\0')
			break;

		int start = i;
		while (path[i] != '/' && path[i] != '\0')
			i++;
		int len = i - start;

		if (path_elem_eq(path + start, len, ".")) {
			continue;
		} else if (path_elem_eq(path + start, len, "..")) {
			pop_path_elem(out, &out_len);
			continue;
		}

		append_path_elem(out, &out_len, path + start, len);
	}

	strncpy(dst, out, PATH_MAX);
	dst[PATH_MAX - 1] = '\0';
}

static void make_absolute_path(char *dst, const char *cwd, const char *path)
{
	char raw[PATH_MAX] = {0};
	int i = 0;
	int j = 0;

	if (path[0] == '/') {
		while (path[i] != '\0' && i < PATH_MAX - 1) {
			raw[i] = path[i];
			i++;
		}
		raw[i] = '\0';
		normalize_absolute_path(dst, raw);
		return;
	}

	while (cwd[i] != '\0' && i < PATH_MAX - 1) {
		raw[i] = cwd[i];
		i++;
	}
	if (i > 1 && i < PATH_MAX - 1) {
		raw[i++] = '/';
	}
	while (path[j] != '\0' && i < PATH_MAX - 1) {
		raw[i++] = path[j++];
	}
	raw[i] = '\0';
	normalize_absolute_path(dst, raw);
}

uint64 sys_write()
{
	LOG_TRACE("sys_write called");

	char buf[256];
	struct Process *current_proc = get_proc();

	int fd;
	uint64 user_ptr;
	int total;

	argint(ARG0, &fd);
	argaddr(ARG1, &user_ptr);
	argint(ARG2, &total);

	if (total < 0) {
		return -1;
	}

	if (fd < 0 || fd >= NOFILE) {
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

	acquire(&ftable_lock);
	if (file->append) {
		file->offset = file->node->size;
	}
	release(&ftable_lock);

	if (file->type == FILE_PIPE) {
		if (file->pipe == 0) {
			LOG_ERROR("sys_write: pipe %d not open", fd);
			return -1;
		}
		int n = pipe_write(file->pipe, (uint8 *) user_ptr, total);
		return n;
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

		if (copyin(current_proc->pagetable, buf, user_ptr, output) <
		    0) {
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

	LOG_TRACE("sys_write returned %d", total - reset);
	return total - reset;
}

uint64 sys_read()
{
	int fd;
	int size;
	uint64 dest;
	char buf[256] = {0};

	argint(ARG0, &fd);
	argaddr(ARG1, &dest);
	argint(ARG2, &size);

	if (size < 0) {
		return -1;
	}

	if (fd < 0 || fd >= NOFILE) {
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

	if (file->type == FILE_PIPE) {
		if (file->pipe == 0) {
			LOG_ERROR("sys_read: pipe not open");
			return -1;
		}
		int n = pipe_read(file->pipe, (uint8 *) dest, size);
		return n;
	}

	if (file->node == 0 || file->node->default_f_ops == 0 ||
	    file->node->default_f_ops->read == 0) {
		LOG_ERROR("sys_read: file %d has no read op", fd);
		return -1;
	}

	// allow read from stream and return not full size
	int is_stream = file->node->type == VFS_DEV;
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

		if (copyout(current_proc->pagetable, (char *) dest,
			    (uint64) buf, len) < 0) {
			LOG_WARN("sys_read: copyout failed");
			return -1;
		}
		dest += len;
		reset -= len;
		total_read += len;

		if (is_stream) {
			break;
		}
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
	char path[PATH_MAX] = {0};
	uint64 uargv;
	uint64 uenvp;
	char (*kargv)[PATH_MAX] = kalloc();
	int argc = 0;
	int ret;

	if (kargv == 0)
		return -1;
	memset(kargv, 0, MAX_EXEC_ARGS * PATH_MAX);

	if (argstr(ARG0, path, PATH_MAX) < 0) {
		kfree(kargv);
		return -1;
	}
	argaddr(ARG1, &uargv);
	argaddr(ARG2, &uenvp);
	if (uenvp != 0) {
		kfree(kargv);
		return -1;
	}

	struct Process *p = get_proc();

	if (uargv == 0) {
		strncpy(kargv[0], path, PATH_MAX);
		kargv[0][PATH_MAX - 1] = '\0';
		argc = 1;
		ret = execve_kernel(path, kargv, argc);
		kfree(kargv);
		return ret < 0 ? ret : argc;
	}

	for (argc = 0; argc < MAX_EXEC_ARGS; argc++) {
		uint64 uargp;

		if (copyin(p->pagetable, (char *) &uargp,
			   uargv + (argc * sizeof(uint64)),
			   sizeof(uint64)) < 0) {
			kfree(kargv);
			return -1;
		}

		if (uargp == 0)
			break;
		if (fetch_user_str(p->pagetable, kargv[argc], uargp, PATH_MAX) <
		    0) {
			kfree(kargv);
			return -1;
		}
	}

	if (argc == 0 || argc >= MAX_EXEC_ARGS) {
		kfree(kargv);
		return -1;
	}

	ret = execve_kernel(path, kargv, argc);
	kfree(kargv);
	return ret < 0 ? ret : argc;
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

	if (copyout(p->pagetable, (char *) buf, (uint64) p->cwd, len) < 0)
		return -1;

	return buf;
}

uint64 sys_chdir()
{
	char path[PATH_MAX];
	if (argstr(ARG0, path, PATH_MAX) < 0)
		return -1;
	if (path[0] == '\0')
		return -1;

	struct Process *p = get_proc();
	char fullpath[PATH_MAX];
	make_absolute_path(fullpath, p->cwd, path);

	if (strcmp(fullpath, "/") == 0) {
		strncpy(p->cwd, fullpath, PATH_MAX);
		p->cwd[PATH_MAX - 1] = '\0';
		return 0;
	}

	struct vfs_inode *node = vfs_lookup_at(vfs_root, fullpath);
	if (node != 0 && node->type == VFS_DIR) {
		strncpy(p->cwd, fullpath, PATH_MAX);
		p->cwd[PATH_MAX - 1] = '\0';
		return 0;
	}

	return -1;
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

	return mkdirat(dirfd, path, mode);
}

uint64 sys_unlinkat()
{
	int dirfd;
	int flags;
	char path[PATH_MAX];

	argint(ARG0, &dirfd);
	argint(ARG2, &flags);

	if (argstr(ARG1, path, PATH_MAX) < 0)
		return -1;
	return unlinkat(dirfd, path, flags);
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
	int fd[2] = {-1, -1};
	uint64 fdaddr;
	int flags;
	struct file *read;
	struct file *write;

	argaddr(ARG0, &fdaddr);
	argint(ARG1, &flags);

	if (flags != 0) {
		LOG_ERROR("pipe2: flags not supported");
		return -1;
	}

	struct Process *p = get_proc();
	if (pipe_alloc(&read, &write)) {
		LOG_ERROR("pipe_alloc failed");
		return -1;
	}

	if ((fd[0] = alloc_fd(p, read)) < 0 ||
	    (fd[1] = alloc_fd(p, write)) < 0) {
		LOG_ERROR("alloc_fd failed");
		goto fail;
	}

	if (copyout(p->pagetable, (char *) fdaddr, (uint64) fd,
		    sizeof(int) * 2) < 0) {
		LOG_ERROR("copyout failed");
		goto fail;
	}

	return 0;

fail:
	acquire(&p->lock);

	if (fd[0] >= 0)
		p->ofile[fd[0]] = 0;
	if (fd[1] >= 0)
		p->ofile[fd[1]] = 0;

	release(&p->lock);

	fileclose(read);
	fileclose(write);

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
	if (oldfd < 0 || oldfd >= NOFILE || proc->ofile[oldfd] == 0) {
		LOG_WARN("sys_dup3: oldfd=%d is not valid", oldfd);
		return -1;
	}
	if (newfd < 0 || newfd >= NOFILE) {
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
