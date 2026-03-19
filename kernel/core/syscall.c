#include "kernel/syscall.h"
#include "asm/mm.h"
#include "core/proc.h"
#include "kernel/log.h"
#include "kernel/types.h"

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

extern uint64 sys_write();
extern uint64 sys_fork();
extern uint64 sys_exit();

// WARNING: Note that both global and static variables are mapped to low memory
// addresses by the linker, and direct access to them will result in errors.

// NOTE: According to the linker's conventions, global arrays are still stored
// using PC-based addressing, which means that the functions within the array
// are located at low memory addresses; therefore, their function addresses need
// to be shifted to higher addresses.
static uint64 (*syscalls[])() = {
    [SYS_write] = sys_write, [SYS_fork] = sys_fork, [SYS_exit] = sys_exit};

static char *syscall_names[] = {[SYS_write] = "write", [SYS_fork] = "fork", [SYS_exit] = "exit"};

extern struct trapframe *mytrapframe;
void syscall() {
  LOG_TRACE("syscall %s", syscall_names[mytrapframe->a7]);
  uint64 num = mytrapframe->a7;
  if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // uint64 (*sys_func)(void) = (uint64 (*)(void))syscalls[num];

    // 2. Cast the pointer to an integer, use a macro to convert it to a
    // high-memory address, and then recast it to a function pointer type that
    // returns a value
    // sys_func = (uint64 (*)(void))ADR2HIGH((uint64)sys_func);

    // 3.  Safely jump to a high-memory address to execute a system call and
    // receive a uint64 return value
    // uint64 ret = sys_func();

    mytrapframe->a0 = syscalls[num]();
    LOG_TRACE("syscall %s: a0: %d  done.", syscall_names[num], mytrapframe->a0);
  } else {
    LOG_ERROR("Unknown syscall %d", num);
    mytrapframe->a0 = -1;
  }
}
