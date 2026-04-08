#include "kernel/sysfile.h"
#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/fcntl.h"
#include "kernel/log.h"
#include "kernel/spinlock.h"
#include "kernel/types.h"
#define NFILE 128

uint64 sys_write() {
  LOG_TRACE("sys_write called");

  char buf[256];
  struct Process *current_proc = get_proc();

  int fd;
  argint(0, &fd);
  char *user_ptr;
  argaddr(1, (uint64 *)&user_ptr);
  int total;
  argint(2, &total);
  if (total < 0) {
    return 0;
  }

  if (fd < 0 || fd >= NFILE) {
    return -1;
  }

  int reset = total;
  int output = 0;

  file_t *file = current_proc->ofile[fd];
  if (file == 0) {
    LOG_ERROR("sys_write: file %d not open", fd);
    return -1;
  }
  if (!(file->writable & O_WRONLY)) {
    return -1;
  }

  while (reset > 0) {
    if (reset >= (int)sizeof(buf)) {
      output = sizeof(buf) - 1;
    } else {
      output = reset;
    }

    if (!copyin(current_proc->pagetable, buf, (uint64)user_ptr, output)) {
      LOG_WARN("sys_write: copyin failed");
      return 0;
    }

    buf[output] = '\0';
    int len =
        file->node->ops->write(file->node, file->offset, output, (uint8 *)buf);
    file->offset += len;

    user_ptr += len;
    reset -= len;
  }

  LOG_TRACE("sys_write returned %d", total);
  return total;
}

uint64 sys_read() {
  int fd, size;
  argint(0, &fd);
  argint(2, &size);
  char *dest;
  argaddr(1, (uint64 *)&dest);

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
  file_t *file = current_proc->ofile[fd];

  if (file == 0) {
    LOG_ERROR("sys_read: file %d not open", fd);
    return -1;
  }
  if (!file->readable) {
    return -1;
  }

  char buf[256];

  while (reset > 0) {
    if (reset >= (int)sizeof(buf)) {
      output = sizeof(buf) - 1;
    } else {
      output = reset;
    }

    int len =
        file->node->ops->read(file->node, file->offset, output, (uint8 *)buf);
    if (len <= 0)
      break;
    file->offset += len;
    buf[len] = '\0';

    if (!copyout(current_proc->pagetable, dest, (uint64)buf, len)) {
      LOG_WARN("sys_write: copyin failed");
      return 0;
    }
    dest += len;
    reset -= len;
    total_read += len;
  }

  return total_read;
}

uint64 sys_close() {
  int fd;
  argint(0, &fd);

  struct Process *proc = get_proc();
  if (fd < 0 || fd >= NFILE || proc->ofile[fd] == 0) {
    return -1;
  }

  file_t *file = proc->ofile[fd];
  proc->ofile[fd] = 0;

  extern struct spinlock ftable_lock;
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
uint64 sys_dup() {
  int oldfd;
  argint(0, &oldfd);
  LOG_TRACE("sys_dup: oldfd=%d", oldfd);
  if (oldfd < 0 || oldfd >= NFILE) {
    return -1;
  }

  struct Process *proc = get_proc();
  if (proc->ofile[oldfd] == 0) {
    return -1;
  }

  extern struct spinlock ftable_lock;
  acquire(&ftable_lock);

  int newfd = alloc_fd(proc, proc->ofile[oldfd]);
  if (newfd == -1) {
    release(&ftable_lock);
    return -1;
  }
  proc->ofile[newfd]->ref_count++;
  release(&ftable_lock);

  LOG_TRACE("sys_dup: newfd=%d, newfile_ref=%d, oldfile_ref=%d", newfd,
            proc->ofile[newfd]->ref_count, proc->ofile[oldfd]->ref_count);
  return newfd;
}

uint64 sys_open() {
  uint64 u_path;
  argaddr(0, &u_path);
  int flags;
  argint(1, (int *)&flags);

  char path[128];
  argstr(0, path, 128);

  LOG_TRACE("sys_open: path=%s", path);
  extern vfs_node_t *vfs_root;
  vfs_node_t *node = vfs_lookup(vfs_root, path);
  if (node == 0)
    return -1;

  int file_id = file_alloc();
  if (file_id == -1)
    return -1;

  extern file_t ftable[NFILE];
  file_t *f = &ftable[file_id];

  f->node = node;
  f->offset = 0;
  f->ref_count = 1;

  int mode = flags & O_ACCMODE;

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
  int fd = alloc_fd(get_proc(), f);
  LOG_TRACE("sys_open: %s -> %d", path, fd);
  return (uint64)fd;
}
