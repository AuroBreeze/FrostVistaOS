#include "kernel/arch/cpu.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/proc.h"
#include "kernel/spinlock.h"

struct cpu cpus[NCPU];
struct Process proc[NPROC];

struct spinlock pid_lock = {
    .name = "pid_lock",
    .locked = 0,
    .cpu = 0,
};
int pid = 1;

int cpuid(void)
{
	int id = arch_get_cpu_id();

	if (id < 0 || id >= NCPU)
		panic("cpuid: invalid cpu id");

	return id;
}

struct cpu *get_cpu(void)
{
	return &cpus[cpuid()];
}

struct Process *get_proc(void)
{
	return get_cpu()->proc;
}

void procinit(void)
{
	for (struct Process *p = proc; p < &proc[NPROC]; p++) {
		p->state = UNUSED;
		initlock(&p->lock, "proc");
	}
}

int get_pid(void)
{
	int value;

	acquire(&pid_lock);
	value = pid++;
	release(&pid_lock);

	return value;
}
