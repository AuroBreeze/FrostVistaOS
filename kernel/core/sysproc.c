#include "core/proc.h"
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
