#ifndef __ASM_SYSCALL_H__
#define __ASM_SYSCALL_H__

#define ARG0 0
#define ARG1 1
#define ARG2 2

// RISC-V Linux syscall numbers used by musl/glibc test binaries.
#define SYS_getcwd 17
#define SYS_dup 23
#define SYS_openat 56
#define SYS_close 57
#define SYS_read 63
#define SYS_write 64
#define SYS_fstat 80
#define SYS_exit 93
#define SYS_exit_group 94
#define SYS_getpid 172
#define SYS_brk 214
#define SYS_clone 220
#define SYS_execve 221
#define SYS_wait4 260

// Internal compatibility names used by the current kernel code.
#define SYS_open SYS_openat
#define SYS_sbrk SYS_brk
#define SYS_fork SYS_clone
#define SYS_exec SYS_execve
#define SYS_wait SYS_wait4
#define SYS_shutdown 12

#endif
