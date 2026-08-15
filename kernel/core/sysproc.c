
#include "kernel/mm/kmalloc.h"
#include "kernel/signal.h"
#define LOG_MODULE "SYSP"

#include "asm/defs.h"
#include "asm/riscv.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/syscall.h"
#include "platform/defs.h"

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

uint64 sys_fork()
{
	LOG_TRACE("sys_fork called");
	return fork();
}

uint64 sys_exit()
{
	LOG_TRACE("sys_exit called");
	int exit_code;
	argint(ARG0, &exit_code);

	exit(exit_code);
	return 0;
}

uint64 sys_wait4()
{
	LOG_TRACE("sys_wait4 called");
	int pid;
	uint64 wstatus;
	int options;

	argint(ARG0, &pid);
	argaddr(ARG1, &wstatus);
	argint(ARG2, &options);

	if (pid == 0 || pid < -1) {
		LOG_WARN("sys_wait4 not supported for group pid");
		return -1;
	}
	if (options != 0 && options != WNOHANG) {
		LOG_WARN("sys_wait4 only supports WNOHANG or 0 as options");
		return -1;
	}

	int child = wait4(pid, wstatus, options);
	LOG_TRACE("sys_wait4 returned %d", child);

	return child;
}

uint64 sys_getpid()
{
	struct Process *p = get_proc();
	return p->pid;
}

uint64 sys_set_tid_address()
{
	uint64 tidptr;
	argaddr(ARG0, &tidptr);

	struct Process *p = get_proc();
	return p->pid;
}

uint64 sys_getuid()
{
	return 0;
}

uint64 sys_getgid()
{
	return 0;
}

uint64 sys_setgid()
{
	return 0;
}

uint64 sys_setuid()
{
	return 0;
}

uint64 sys_brk()
{
	LOG_TRACE("sys_brk called");
	uint64 addr;

	argaddr(ARG0, &addr);
	return brk(addr);
}

uint64 sys_shutdown()
{
	LOG_INFO("sys_shutdown called");
	sbi_shutdown();
	return 0;
}

uint64 sys_getppid()
{
	struct Process *p = get_proc();

	if (p->parent == 0)
		return 0;

	return p->parent->pid;
}

uint64 sys_gettimeofday()
{
	uint64 tv_addr;
	uint64 tz_addr;
	argaddr(ARG0, &tv_addr);
	argaddr(ARG1, &tz_addr);

	if (tv_addr != 0) {
		uint64 time = r_time();
		struct linux_timeval tv = {
		    .tv_sec = time / 10000000,
		    .tv_usec = (time % 10000000) / 10,
		};

		struct Process *p = get_proc();
		if (copyout(p->pagetable, (char *) tv_addr, (uint64) &tv,
			    sizeof(tv)) < 0) {
			return -1;
		}
	}

	return 0;
}

/**
 * sys_clock_gettime - syscall 88 (clock_gettime)
 *
 * Used by busybox touch to fetch the current time. Only the realtime clock
 * is provided; the clockid argument is accepted and ignored. The value
 * mirrors sys_gettimeofday: sec = time/10000000, nsec = usec*1000.
 * */
uint64 sys_clock_gettime()
{
	uint64 tp_addr;
	argaddr(ARG1, &tp_addr); /* struct timespec * */
	if (tp_addr == 0)
		return -1;

	uint64 time = r_time();
	uint64 sec = time / 10000000;
	uint64 nsec = ((time % 10000000) / 10) * 1000;

	/* struct timespec { long tv_sec; long tv_nsec; } */
	uint64 ts[2] = {sec, nsec};
	struct Process *p = get_proc();
	if (copyout(p->pagetable, (char *) tp_addr, (uint64) ts, sizeof(ts)) <
	    0) {
		return -1;
	}
	return 0;
}

uint64 sys_times()
{
	uint64 tms_addr;
	argaddr(ARG0, &tms_addr);

	if (tms_addr != 0) {
		struct linux_tms tms = {0};
		struct Process *p = get_proc();
		if (copyout(p->pagetable, (char *) tms_addr, (uint64) &tms,
			    sizeof(tms)) < 0) {
			return -1;
		}
	}

	return r_time() / 100000;
}

uint64 sys_uname()
{
	uint64 uts_addr;
	argaddr(ARG0, &uts_addr);

	struct linux_utsname uts = {0};
	strcpy(uts.sysname, "FrostVistaOS");
	strcpy(uts.nodename, "frostvista");
	strcpy(uts.release, "0.6");
	strcpy(uts.version, "oscomp");
	strcpy(uts.machine, "riscv64");
	strcpy(uts.domainname, "local");

	struct Process *p = get_proc();
	if (copyout(p->pagetable, (char *) uts_addr, (uint64) &uts,
		    sizeof(uts)) < 0) {
		return -1;
	}

	return 0;
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

uint64 sys_setpriority()
{
	// Scheduler priority is not implemented yet. Returning success keeps
	// simple ABI probes moving without changing the current scheduler.
	return 0;
}

uint64 sys_nanosleep()
{
	uint64 req_addr;
	argaddr(ARG0, &req_addr);

	if (req_addr == 0)
		return -1;

	struct linux_timespec req;
	struct Process *p = get_proc();
	if (copyin(p->pagetable, (char *) &req, req_addr, sizeof(req)) < 0)
		return -1;

	if (req.tv_nsec >= 1000000000)
		return -1;

	uint64 delta = (req.tv_sec * 10000000) + (req.tv_nsec / 100);
	uint64 deadline = r_time() + delta;
	while (r_time() < deadline) {
		yield();
	}

	return 0;
}

uint64 sys_kill()
{
	int pid;
	int sig;
	argint(ARG0, &pid);
	argint(ARG1, &sig);

	return kill(pid, sig);
}

uint64 sys_rt_sigaction()
{
	int sig;
	uint64 act_addr;
	uint64 old_act_addr;
	int sigsetsize;

	argint(ARG0, &sig);
	argaddr(ARG1, &act_addr);
	argaddr(ARG2, &old_act_addr);
	argint(ARG3, &sigsetsize);

	if (sig < 1 || sig >= NSIG)
		return -1;
	if (sigsetsize != (NSIG / 8)) // musl : _NSIG / 8
		return -1;

	if (sig == SIGKILL || sig == SIGSTOP)
		return -1;

	struct Process *p = get_proc();
	struct sigaction *act = kmalloc(sizeof(struct sigaction));
	if (act == 0)
		return -1;
	if (act_addr && (copyin(p->pagetable, (char *) act, act_addr,
				sizeof(struct sigaction)) < 0)) {
		kmfree(act);
		return -1;
	}

	acquire(&p->lock);
	if (old_act_addr != 0) {
		if (copyout(p->pagetable, (char *) old_act_addr,
			    (uint64) &p->sighand.actions[sig],
			    sizeof(struct sigaction)) < 0) {
			release(&p->lock);
			kmfree(act);
			return -1;
		}
	}
	p->sighand.actions[sig] = *act;
	release(&p->lock);
	kmfree(act);

	return 0;
}

uint64 sys_rt_sigprocmask()
{
	int how;
	uint64 set_addr;
	uint64 old_set_addr;
	int sigsetsize;

	argint(ARG0, &how);
	argaddr(ARG1, &set_addr);
	argaddr(ARG2, &old_set_addr);
	argint(ARG3, &sigsetsize);

	if (sigsetsize != (NSIG / 8)) // musl : _NSIG / 8
		return -1;
	if (how < SIG_BLOCK || how > SIG_SETMASK)
		return -1;

	struct Process *p = get_proc();

	// Snapshot the old mask before any change: old_set must report the
	// mask as it was before this call.
	uint64 old = p->sighand.sig_blocked;

	if (set_addr != 0) {
		uint64 new_set;
		if (copyin(p->pagetable, (char *) &new_set, set_addr,
			   sizeof(new_set)) < 0)
			return -1;

		// POSIX: SIGKILL/SIGSTOP can never be blocked, even if the
		// caller asked for them.
		new_set &= ~((1UL << SIGKILL) | (1UL << SIGSTOP));

		acquire(&p->lock);
		switch (how) {
		case SIG_BLOCK:
			p->sighand.sig_blocked |= new_set;
			break;
		case SIG_UNBLOCK:
			p->sighand.sig_blocked &= ~new_set;
			break;
		case SIG_SETMASK:
			p->sighand.sig_blocked = new_set;
			break;
		default:
			return -1;
		}
		release(&p->lock);
	}

	if (old_set_addr != 0) {
		if (copyout(p->pagetable, (char *) old_set_addr, old,
			    sizeof(old)) < 0)
			return -1;
	}

	return 0;
}
