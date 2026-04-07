#include "kernel/log.h"

uint64 sys_fork() {
  LOG_TRACE("sys_fork called");
  return fork();
}

uint64 sys_exit() {
  LOG_TRACE("sys_exit called");
  exit();
  return 0;
}

uint64 sys_wait() {
  LOG_TRACE("sys_wait called");
  int pid = wait();
  LOG_DEBUG("sys_wait returned %d", pid);
  return pid;
}

uint64 sys_sbrk() {
  LOG_TRACE("sys_sbrk called");
  int64 size;
  argint(0, (int *)&size);

  return sbrk(size);
}
