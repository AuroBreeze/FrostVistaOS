#include "kernel/syscall.h"
#include "core/proc.h"
#include "kernel/log.h"
#include "kernel/types.h"

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

extern uint64 sys_write();
extern uint64 sys_fork();
extern uint64 sys_exit();
extern uint64 sys_wait();
extern uint64 sys_sbrk();

// Because the linker has been modified, static variables are now located at
// virtual addresses.
static uint64 (*syscalls[])() = {[SYS_write] = sys_write,
                                 [SYS_fork] = sys_fork,
                                 [SYS_exit] = sys_exit,
                                 [SYS_wait] = sys_wait,
                                 [SYS_sbrk] = sys_sbrk};

static char *syscall_names[] = {[SYS_write] = "write",
                                [SYS_fork] = "fork",
                                [SYS_exit] = "exit",
                                [SYS_wait] = "wait",
                                [SYS_sbrk] = "sbrk"};

void syscall() {
  struct Process *current_proc = get_proc();
  struct trapframe *trapframe = current_proc->trapframe;

  LOG_TRACE("syscall %s", syscall_names[trapframe->a7]);
  uint64 num = trapframe->a7;
  if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    trapframe->a0 = syscalls[num]();
    LOG_TRACE("syscall %s: a0: %d  done.", syscall_names[num], trapframe->a0);
  } else {
    LOG_ERROR("Unknown syscall %d", num);
    trapframe->a0 = -1;
  }
}
