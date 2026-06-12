#include "asm/defs.h"
#include "asm/riscv.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/syscall.h"
#include "platform/defs.h"

#define LOG_MODULE "SYSP"

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

uint64 sys_sbrk()
{
	LOG_TRACE("sys_sbrk called");
	int size;
	argint(ARG0, &size);

	return sbrk(size);
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
