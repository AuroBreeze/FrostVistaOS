#include "kernel/sysfile.h"
#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"

uint64 sys_write() {
  LOG_TRACE("sys_write called");

  char buf[256];
  struct Process *current_proc = get_proc();
  char *user_ptr = (char *)current_proc->trapframe->a1;

  int cnt = current_proc->trapframe->a2;
  if (cnt < 0) {
    return 0;
  }

  if (cnt >= (int)sizeof(buf)) {
    cnt = sizeof(buf) - 1;
  }

  if (!copyin(current_proc->pagetable, buf, (uint64)user_ptr, cnt)) {
    LOG_WARN("sys_write: copyin failed");
    return 0;
  }

  buf[cnt] = '\0';
  kprintf("%s", buf);

  LOG_TRACE("sys_write returned %d", cnt);
  return cnt;
}
