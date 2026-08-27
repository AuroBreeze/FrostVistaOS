#include "kernel/defs.h"
#include "kernel/proc.h"
#include "kernel/spinlock.h"

void sleep(void *chan, struct spinlock *lk)
{
	struct Process *p = get_proc();

	/* Acquire the process lock before releasing the caller's lock. */
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
	struct Process *current = get_proc();

	for (int i = 0; i < NPROC; i++) {
		struct Process *p = &proc[i];

		acquire(&p->lock);
		if (p != current && p->chan == chan && p->state == SLEEPING) {
			p->state = RUNNABLE;
		}
		release(&p->lock);
	}
}
