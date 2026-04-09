#include "kernel/syscall.h"
#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

// PERF: Copy the smaller of the page boundary or len, then search for \0 to
// avoid searching for a single character
static int fetch_user_str(pagetable_t pagetable, char *dst, uint64 src_va,
                          uint64 max_len) {
  for (uint64 i = 0; i < max_len; i++) {
    if (copyin(pagetable, (char *)&dst[i], src_va + i, 1) == 0) {
      return -1;
    }
    if (dst[i] == '\0') {
      return i;
    }
  }
  return -1;
}

// xv6
// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
static int fetchstr(uint64 addr, char *buf, int max) {
  struct Process *p = get_proc();
  if (fetch_user_str(p->pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

// xv6
static uint64 argraw(int n) {
  struct Process *p = get_proc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// xv6
// Fetch the nth 32-bit system call argument.
void argint(int n, int *ip) { *ip = argraw(n); }

// xv6
// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
void argaddr(int n, uint64 *ip) { *ip = argraw(n); }

// xv6
// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int argstr(int n, char *buf, int max) {
  uint64 addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}

extern uint64 sys_write();
extern uint64 sys_fork();
extern uint64 sys_exit();
extern uint64 sys_wait();
extern uint64 sys_sbrk();
extern uint64 sys_open();
extern uint64 sys_read();
extern uint64 sys_close();
extern uint64 sys_dup();
extern uint64 sys_fstat();

// Because the linker has been modified, static variables are now located at
// virtual addresses.
static uint64 (*syscalls[])() = {
    [SYS_write] = sys_write, [SYS_fork] = sys_fork,   [SYS_exit] = sys_exit,
    [SYS_wait] = sys_wait,   [SYS_sbrk] = sys_sbrk,   [SYS_open] = sys_open,
    [SYS_read] = sys_read,   [SYS_close] = sys_close, [SYS_dup] = sys_dup,
    [SYS_fstat] = sys_fstat,
};

static char *syscall_names[] = {
    [SYS_write] = "write", [SYS_fork] = "fork",   [SYS_exit] = "exit",
    [SYS_wait] = "wait",   [SYS_sbrk] = "sbrk",   [SYS_open] = "open",
    [SYS_read] = "read",   [SYS_close] = "close", [SYS_dup] = "dup",
    [SYS_fstat] = "fstat",
};

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
