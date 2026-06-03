#ifndef __USER_H__
#define __USER_H__

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define AT_FDCWD -100

// RISC-V Linux syscall numbers used by the kernel syscall dispatcher.
#define SYS_getcwd 17
#define SYS_dup 23
#define SYS_dup3 24
#define SYS_pipe2 59
#define SYS_chdir 49
#define SYS_openat 56
#define SYS_close 57
#define SYS_lseek 62
#define SYS_read 63
#define SYS_write 64
#define SYS_fstat 80
#define SYS_exit 93
#define SYS_exit_group 94
#define SYS_getpid 172
#define SYS_getppid 173
#define SYS_brk 214
#define SYS_clone 220
#define SYS_execve 221
#define SYS_wait4 260
#define SYS_gettimeofday 169
#define SYS_times 153
#define SYS_uname 160
#define SYS_nanosleep 101
#define SYS_sched_yield 124
#define SYS_setpriority 140

// Keep the local test wrappers readable while using Linux ABI numbers.
#define SYS_open SYS_openat
#define SYS_sbrk SYS_brk
#define SYS_fork SYS_clone
#define SYS_exec SYS_execve
#define SYS_wait SYS_wait4
#define SYS_unlinkat 35
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

struct linux_timeval {
	uint64 tv_sec;
	uint64 tv_usec;
};

struct linux_timespec {
	uint64 tv_sec;
	uint64 tv_nsec;
};

struct linux_tms {
	uint64 tms_utime;
	uint64 tms_stime;
	uint64 tms_cutime;
	uint64 tms_cstime;
};

struct linux_utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};

// --- System Call Wrappers ---
long write(int fd, const char *buf, uint64 count);
void exit(int status) __attribute__((noreturn));
void *sbrk(int increment);
int fork(void);
int wait(void);
int exec(const char *path);
int open(const char *path, int flags);
int chdir(const char *path);
long read(int fd, void *buf, uint64 count);
int close(int fd);
int dup(int fd);
int getpid(void);
int getppid(void);
long getcwd(char *buf, int size);
long gettimeofday(struct linux_timeval *tv, void *tz);
long times(struct linux_tms *tms);
long uname(struct linux_utsname *uts);
long nanosleep(struct linux_timespec *req);
long sched_yield(void);
long setpriority(int which, int who, int prio);
long lseek(int fd, long offset, int whence);
int dup3(int oldfd, int newfd, int flags);
int pipe2(int *fds, int flags);
int unlinkat(int dirfd, const char *path, int flags);
int unlink(const char *path);
void shutdown(void) __attribute__((noreturn));

// --- Simple Library Functions ---
void printf(const char *fmt, ...);
void print_int(int num);
unsigned long strlen(const char *s);
void *memcpy(void *dst, const void *src, uint64 n);
void *memset(void *dst, int c, uint64 n);

#endif
