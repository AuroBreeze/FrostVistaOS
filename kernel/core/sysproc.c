#include "core/proc.h"
#include "kernel/log.h"

uint64 sys_fork() {
  LOG_TRACE("sys_fork called");
  return fork();
}
