#include "kernel/syscall.h"
#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"

#define LOG_MODULE "SYSC"

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

// PERF: Copy the smaller of the page boundary or len, then search for \0 to
// avoid searching for a single character
int fetch_user_str(pagetable_t pagetable, char *dst, uint64 src_va,
		   uint64 max_len)
{
	for (uint64 i = 0; i < max_len; i++) {
		if (copyin(pagetable, &dst[i], src_va + i, 1) < 0) {
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
static int fetchstr(uint64 addr, char *buf, int max)
{
	struct Process *p = get_proc();
	if (fetch_user_str(p->pagetable, buf, addr, max) < 0)
		return -1;
	return strlen(buf);
}

// xv6
static uint64 argraw(int n)
{
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
	default:
		return 0;
	}
	panic("argraw");
	return -1;
}

// xv6
// Fetch the nth 32-bit system call argument.
void argint(int n, int *ip)
{
	*ip = argraw(n);
}

// xv6
// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
void argaddr(int n, uint64 *ip)
{
	*ip = argraw(n);
}

// xv6
// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int argstr(int n, char *buf, int max)
{
	uint64 addr;
	argaddr(n, &addr);
	return fetchstr(addr, buf, max);
}

extern uint64 sys_write();
extern uint64 sys_fork();
extern uint64 sys_exit();
extern uint64 sys_wait4();
extern uint64 sys_getpid();
extern uint64 sys_brk();
extern uint64 sys_open();
extern uint64 sys_openat();
extern uint64 sys_read();
extern uint64 sys_close();
extern uint64 sys_dup();
extern uint64 sys_fstat();
extern uint64 sys_exec();
extern uint64 sys_shutdown();
extern uint64 sys_getppid();
extern uint64 sys_gettimeofday();
extern uint64 sys_times();
extern uint64 sys_uname();
extern uint64 sys_nanosleep();
extern uint64 sys_sched_yield();
extern uint64 sys_getcwd();
extern uint64 sys_chdir();
extern uint64 sys_mkdirat();
extern uint64 sys_unlinkat();
extern uint64 sys_linkat();
extern uint64 sys_getdents64();
extern uint64 sys_pipe2();
extern uint64 sys_lseek();
extern uint64 sys_newfstatat();
extern uint64 sys_mmap();
extern uint64 sys_munmap();
extern uint64 sys_mount();
extern uint64 sys_umount2();
extern uint64 sys_dup3();
extern uint64 sys_setpriority();

// Because the linker has been modified, static variables are now located at
// virtual addresses.
static uint64 (*syscalls[])() = {
    [SYS_write] = sys_write,
    [SYS_fork] = sys_fork,
    [SYS_exit] = sys_exit,
    [SYS_exit_group] = sys_exit,
    [SYS_wait4] = sys_wait4,
    [SYS_getpid] = sys_getpid,
    [SYS_brk] = sys_brk,
    [SYS_open] = sys_openat,
    [SYS_read] = sys_read,
    [SYS_close] = sys_close,
    [SYS_dup] = sys_dup,
    [SYS_fstat] = sys_fstat,
    [SYS_exec] = sys_exec,
    [SYS_shutdown] = sys_shutdown,
    [SYS_getppid] = sys_getppid,
    [SYS_gettimeofday] = sys_gettimeofday,
    [SYS_times] = sys_times,
    [SYS_uname] = sys_uname,
    [SYS_nanosleep] = sys_nanosleep,
    [SYS_sched_yield] = sys_sched_yield,
    [SYS_getcwd] = sys_getcwd,
    [SYS_chdir] = sys_chdir,
    [SYS_mkdirat] = sys_mkdirat,
    [SYS_unlinkat] = sys_unlinkat,
    [SYS_linkat] = sys_linkat,
    [SYS_getdents64] = sys_getdents64,
    [SYS_pipe2] = sys_pipe2,
    [SYS_lseek] = sys_lseek,
    [SYS_newfstatat] = sys_newfstatat,
    [SYS_mmap] = sys_mmap,
    [SYS_munmap] = sys_munmap,
    [SYS_mount] = sys_mount,
    [SYS_umount2] = sys_umount2,
    [SYS_dup3] = sys_dup3,
    [SYS_setpriority] = sys_setpriority,
};

static char *syscall_names[] = {
    [SYS_write] = "write",
    [SYS_fork] = "fork",
    [SYS_exit] = "exit",
    [SYS_exit_group] = "exit_group",
    [SYS_wait4] = "wait4",
    [SYS_getpid] = "getpid",
    [SYS_brk] = "brk",
    [SYS_open] = "open",
    [SYS_read] = "read",
    [SYS_close] = "close",
    [SYS_dup] = "dup",
    [SYS_fstat] = "fstat",
    [SYS_exec] = "exec",
    [SYS_shutdown] = "shutdown",
    [SYS_getppid] = "getppid",
    [SYS_gettimeofday] = "gettimeofday",
    [SYS_times] = "times",
    [SYS_uname] = "uname",
    [SYS_nanosleep] = "nanosleep",
    [SYS_sched_yield] = "sched_yield",
    [SYS_getcwd] = "getcwd",
    [SYS_chdir] = "chdir",
    [SYS_mkdirat] = "mkdirat",
    [SYS_unlinkat] = "unlinkat",
    [SYS_linkat] = "linkat",
    [SYS_getdents64] = "getdents64",
    [SYS_pipe2] = "pipe2",
    [SYS_lseek] = "lseek",
    [SYS_newfstatat] = "newfstatat",
    [SYS_mmap] = "mmap",
    [SYS_munmap] = "munmap",
    [SYS_mount] = "mount",
    [SYS_umount2] = "umount2",
    [SYS_dup3] = "dup3",
    [SYS_setpriority] = "setpriority",
};

void syscall()
{
	struct Process *current_proc = get_proc();
	struct trapframe *trapframe = current_proc->trapframe;

	uint64 num = trapframe->a7;
	if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
		LOG_TRACE("syscall %s", syscall_names[num]);
		trapframe->a0 = syscalls[num]();
		LOG_TRACE("syscall %s: a0: %d  done.", syscall_names[num],
			  trapframe->a0);
	} else {
		LOG_ERROR("Unknown syscall %d", num);
		trapframe->a0 = -1;
	}
}
