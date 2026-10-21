#include "user/user.h"
#include "asm/user.h"

static inline long syscall(long num, long a0, long a1, long a2)
{
	return arch_user_syscall3(num, a0, a1, a2);
}

static inline long syscall4(long num, long a0, long a1, long a2, long a3)
{
	return arch_user_syscall4(num, a0, a1, a2, a3);
}

static inline long syscall6(long num, long a0, long a1, long a2, long a3,
			    long a4, long a5)
{
	return arch_user_syscall6(num, a0, a1, a2, a3, a4, a5);
}

long write(int fd, const char *buf, uint64 count)
{
	return syscall(SYS_write, fd, (long) buf, count);
}
void exit(int status)
{
	syscall(SYS_exit, status, 0, 0);
	while (1)
		;
}
long brk(void *addr)
{
	return syscall(SYS_brk, (long) addr, 0, 0);
}

int fork(void)
{
	return syscall(SYS_fork, 0, 0, 0);
}
int wait(void)
{
	return syscall(SYS_wait, -1, 0, 0);
}

int waitpid(int pid, int *status, int options)
{
	return syscall(SYS_wait4, pid, (long) status, options);
}
void *mmap(void *addr, uint64 len, int prot, int flags, int fd, uint64 offset)
{
	return (void *) syscall6(SYS_mmap, (long) addr, len, prot, flags, fd,
				 offset);
}
int munmap(void *addr, uint64 len)
{
	return syscall(SYS_munmap, (long) addr, len, 0);
}
int exec(const char *path)
{
	return syscall(SYS_exec, (long) path, 0, 0);
}

int execve(const char *path, char *const argv[], char *const envp[])
{
	return syscall(SYS_exec, (long) path, (long) argv, (long) envp);
}

int open(const char *path, int flags)
{
	return (int) syscall(SYS_open, AT_FDCWD, (long) path, flags);
}
int chdir(const char *path)
{
	return (int) syscall(SYS_chdir, (long) path, 0, 0);
}

int mkdir(const char *path, int mode)
{
	return (int) syscall(SYS_mkdirat, AT_FDCWD, (long) path, mode);
}

long read(int fd, void *buf, uint64 count)
{
	return syscall(SYS_read, fd, (long) buf, count);
}
int close(int fd)
{
	return (int) syscall(SYS_close, fd, 0, 0);
}

int dup(int fd)
{
	return (int) syscall(SYS_dup, fd, 0, 0);
}

int getpid(void)
{
	return (int) syscall(SYS_getpid, 0, 0, 0);
}

int getppid(void)
{
	return (int) syscall(SYS_getppid, 0, 0, 0);
}

int kill(int pid, int sig)
{
	return (int) syscall(SYS_kill, pid, sig, 0);
}

int rt_sigaction(int sig, const struct sigaction *act, struct sigaction *oldact)
{
	return (int) syscall4(SYS_rt_sigaction, sig, (long) act, (long) oldact,
			      sizeof(uint64));
}

int rt_sigprocmask(int how, const uint64 *set, uint64 *oldset)
{
	return (int) syscall4(SYS_rt_sigprocmask, how, (long) set,
			      (long) oldset, sizeof(uint64));
}

int rt_sigpending(uint64 *set)
{
	return (int) syscall(SYS_rt_sigpending, (long) set, 0, 0);
}

long getcwd(char *buf, int size)
{
	return syscall(SYS_getcwd, (long) buf, size, 0);
}

long gettimeofday(struct linux_timeval *tv, void *tz)
{
	return syscall(SYS_gettimeofday, (long) tv, (long) tz, 0);
}

long clock_gettime(int clock_id, struct linux_timespec *ts)
{
	return syscall(SYS_clock_gettime, clock_id, (long) ts, 0);
}

long times(struct linux_tms *tms)
{
	return syscall(SYS_times, (long) tms, 0, 0);
}

long uname(struct linux_utsname *uts)
{
	return syscall(SYS_uname, (long) uts, 0, 0);
}

long nanosleep(struct linux_timespec *req)
{
	return syscall(SYS_nanosleep, (long) req, 0, 0);
}

long sched_yield(void)
{
	return syscall(SYS_sched_yield, 0, 0, 0);
}

long setpriority(int which, int who, int prio)
{
	return syscall(SYS_setpriority, which, who, prio);
}

long lseek(int fd, long offset, int whence)
{
	return syscall(SYS_lseek, fd, offset, whence);
}

int dup3(int oldfd, int newfd, int flags)
{
	return (int) syscall(SYS_dup3, oldfd, newfd, flags);
}

int pipe2(int *fds, int flags)
{
	return (int) syscall(SYS_pipe2, (long) fds, flags, 0);
}

int unlinkat(int dirfd, const char *path, int flags)
{
	return (int) syscall(SYS_unlinkat, dirfd, (long) path, flags);
}

long getdents64(int fd, void *buf, int count)
{
	return syscall(SYS_getdents64, fd, (long) buf, count);
}

void shutdown(void)
{
	syscall(SYS_shutdown, 0, 0, 0);
	while (1)
		;
}

void terminal_claim_input(int pid)
{
	syscall(SYS_terminal_claim_input, pid, 0, 0);
}
