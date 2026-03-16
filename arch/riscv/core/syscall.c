#include "asm/syscall.h"
#include "core/proc.h"
#include "kernel/log.h"
#include "kernel/types.h"

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

extern uint64 sys_write(uint64 *regs);

static uint64 (*syscalls[])(uint64 *regs) = {[SYS_write] = sys_write};

static char *syscall_names[] = {[SYS_write] = "write"};

extern struct trapframe *mytrapframe;
void syscall() {
  uint64 num = mytrapframe->a7;
  if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    mytrapframe->a0 = syscalls[num]((uint64 *)mytrapframe);
    LOG_TRACE("syscall %s: %d", syscall_names[num], mytrapframe->a0);
  } else {
    LOG_ERROR("Unknown syscall %d", num);
    mytrapframe->a0 = -1;
  }
}
