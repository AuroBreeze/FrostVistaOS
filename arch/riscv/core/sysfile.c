#include "core/sysfile.h"
#include "core/proc.h"
#include "driver/hal_console.h"
#include "kernel/log.h"
#include "kernel/types.h"

uint64 sys_write() {
  LOG_TRACE("sys_write called");
  hal_console_puts("Hello from sys_write\n");
  extern struct trapframe *mytrapframe;
  LOG_DEBUG("syscall %d: %d", 1, mytrapframe->a0);
  return 0;
}
