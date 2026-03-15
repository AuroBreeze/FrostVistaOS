#include "asm/syscall.h"
#include "asm/riscv.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

extern uint64 sys_write(uint64 *regs);

static uint64 (*syscalls[])(uint64 *regs) = {[SYS_write] = sys_write};

static char *syscall_names[] = {[SYS_write] = "write"};

void syscall(uint64 *regs) {
  uint64 num = regs[16];
  if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    regs[9] = syscalls[num](regs);
    LOG_TRACE("syscall %s: %d", syscall_names[num], regs[9]);
  } else {
    LOG_ERROR("Unknown syscall %d", num);
    regs[9] = -1;
  }
}
