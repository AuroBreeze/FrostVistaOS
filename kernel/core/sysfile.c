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

  int total = current_proc->trapframe->a2;
  if (total < 0) {
    return 0;
  }

  int reset = total;
  int output = 0;

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
    kprintf("%s", buf);

    user_ptr += output;
    reset -= output;
  }

  LOG_TRACE("sys_write returned %d", total);
  return total;
}
