#include "kernel/spinlock.h"
#include "kernel/arch/irq.h"
#include "kernel/proc.h"
#include "kernel/defs.h"
#include "kernel/log.h"

// Initialize the lock
void initlock(struct spinlock *lk, char *name)
{
	// LOG_TRACE("Initialize lock %s", name);
	lk->name = name;
	lk->locked = 0;
	lk->cpu = 0;
}

// Check whether this cpu is holding the lock
int holding(struct spinlock *lk)
{
	int r;
	push_off();
	r = (lk->locked && lk->cpu == get_cpu());
	pop_off();
	return r;
}

// acquire the lock
void acquire(struct spinlock *lk)
{
	push_off();

	if (holding(lk)) {
		panic("acquire: already holding lock");
	}
	while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
		;
	// Prevent reordering from causing data to be accessed before the lock
	// is acquired
	__sync_synchronize();

	lk->cpu = get_cpu();
}

// release the lock
void release(struct spinlock *lk)
{
	if (!holding(lk)) {
		panic("release: not holding lock");
	}
	lk->cpu = 0;
	__sync_synchronize();
	__sync_lock_release(&lk->locked);

	pop_off();
}

void push_off(void)
{
	int old = arch_irq_save();

	struct cpu *c = get_cpu();
	if (c->noff == 0) {
		c->intena = old;
	}
	c->noff++;
}

void pop_off(void)
{
	if (arch_irq_enabled()) {
		// By default, this is paired with `push_off`, which disables
		// interrupts; therefore, interrupts should still be disabled
		// here.
		panic("pop_off: interrupt enabled\n");
	}
	struct cpu *c = get_cpu();
	if (c->noff < 1) {
		panic("pop_off");
	}
	c->noff--;
	if (c->noff == 0 && c->intena) {
		arch_irq_enable();
	}
}

void sleep(void *chan, struct spinlock *lk)
{
	struct Process *p = get_proc();

	// If the lock belongs to the current process, modifying the current
	// process's data no longer requires a lock request. Otherwise, acquire
	// the lock on the current process and release the lock passed in
	if (lk != &p->lock) {
		acquire(&p->lock);
		release(lk);
	}

	p->chan = chan;
	p->state = SLEEPING;

	sched();

	p->chan = 0;

	if (lk != &p->lock) {
		release(&p->lock);
		acquire(lk);
	}
}

void wakeup(void *chan)
{
	struct Process *p;
	extern struct Process proc[64];

	for (int i = 0; i < 64; i++) {
		p = &proc[i];
		acquire(&p->lock);
		if (p != get_proc() && p->chan == chan &&
		    p->state == SLEEPING) {
			p->state = RUNNABLE;
		}
		release(&p->lock);
	}
}
