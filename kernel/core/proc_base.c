#include "kernel/arch/cpu.h"
#include "kernel/arch/irq.h"
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

/* --- proc.c --- */

/**
 * sched - Switch to the next process
 *
 * Context: Must be holding the proc_lock before calling
 *
 */
void sched(void)
{
	int intena;
	struct Process *p = get_proc();

	if (!holding(&p->lock))
		panic("sched p->lock");
	if (get_cpu()->noff != 1)
		panic("sched locks");
	if (p->state == RUNNING)
		panic("sched running");
	if (arch_irq_enabled())
		panic("sched interruptible");

	intena = get_cpu()->intena;

	extern void swtch(arch_context_t * old, arch_context_t * new);

	// Switch back to the CPU's context
	swtch(p->context, &get_cpu()->context);
	get_cpu()->intena = intena;
}

/**
 * yield - Yield the CPU
 *
 * Context: Will switch back to the CPU's context and return to the scheduler
 */
void yield(void)
{
	struct Process *current_proc = get_proc();

	if (current_proc != 0 && current_proc->state == RUNNING) {
		acquire(&current_proc->lock);
		current_proc->state = RUNNABLE;
		sched();
		release(&current_proc->lock);
	}
}
