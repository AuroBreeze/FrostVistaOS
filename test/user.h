#ifndef __USER_H__
#define __USER_H__

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define AT_FDCWD -100

// RISC-V Linux syscall numbers used by the kernel syscall dispatcher.
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
#define SYS_gettimeofday 169
#define SYS_times 153
#define SYS_uname 160
#define SYS_nanosleep 101
#define SYS_sched_yield 124

// Keep the local test wrappers readable while using Linux ABI numbers.
#define SYS_open SYS_openat
#define SYS_sbrk SYS_brk
#define SYS_fork SYS_clone
#define SYS_exec SYS_execve
#define SYS_wait SYS_wait4
#define SYS_shutdown 12

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

typedef long int64;

typedef uint64 *pagetable_t;
typedef uint64 pte_t;

// --- System Call Wrappers ---
long write(int fd, const char *buf, uint64 count);
void exit(int status) __attribute__((noreturn));
void *sbrk(int increment);
int fork(void);
int wait(void);
int exec(const char *path);
int open(const char *path, int flags);
long read(int fd, void *buf, uint64 count);
int close(int fd);
int dup(int fd);
void shutdown(void) __attribute__((noreturn));

// --- Simple Library Functions ---
void printf(const char *fmt, ...);
void print_int(int num);
unsigned long strlen(const char *s);
void *memcpy(void *dst, const void *src, uint64 n);
void *memset(void *dst, int c, uint64 n);

#endif
