#include "kernel/sysfile.h"
#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/fcntl.h"
#include "kernel/log.h"
#include "kernel/types.h"
#define NFILE 128

uint64 sys_write() {
  LOG_TRACE("sys_write called");

  char buf[256];
  struct Process *current_proc = get_proc();
  int fd = current_proc->trapframe->a0;
  char *user_ptr = (char *)current_proc->trapframe->a1;
  int total = current_proc->trapframe->a2;
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
    // kprintf("%s", buf);
    int len =
        file->node->ops->write(file->node, file->offset, output, (uint8 *)buf);

    user_ptr += len;
    reset -= len;
  }

  LOG_TRACE("sys_write returned %d", total);
  return total;
}

uint64 sys_open() {
  struct Process *p = get_proc();
  uint64 u_path = p->trapframe->a0;
  uint64 flags = p->trapframe->a1;

  char path[128];

  if (copyin(p->pagetable, (char *)path, (uint64)u_path, 128) < 0) {
    return -1;
  }

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
