#include "kernel/sysfile.h"
#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "kernel/defs.h"

uint64 sys_write() {
  LOG_TRACE("sys_write called");

  char buf[256];
  char *user_ptr = (char *)mytrapframe->a1;

  extern struct Process *current_proc;;
  int cnt = mytrapframe->a2;
  if(cnt<0){
    return 0;
  }
  if(cnt >= (int)sizeof(buf)){
    cnt = sizeof(buf) - 1;
  }

  if(!copyin(current_proc->pagetable, buf, (uint64)user_ptr, cnt)){
    LOG_WARN("sys_write: copyin failed");
    return 0;
  }

  buf[cnt] = '\0';
  kprintf("%s", buf);

  LOG_DEBUG("syscall %d: %d", 1, mytrapframe->a0);
  return cnt;
}
