#include "kernel/irq.h"
#include "kernel/defs.h"
#include "kernel/spinlock.h"

struct irq_action {
	irq_handler_t handler;
	void *arg;
};

static struct irq_action actions[IRQ_VECTOR_COUNT][IRQ_SHARED_HANDLERS];
static struct spinlock irq_lock = {.name = "irq_actions"};

int irq_register(uint32 vector, irq_handler_t handler, void *arg)
{
	if (vector >= IRQ_VECTOR_COUNT || !handler)
		return -1;
	acquire(&irq_lock);
	int free_slot = -1;
	for (uint i = 0; i < IRQ_SHARED_HANDLERS; i++) {
		if (actions[vector][i].handler == handler &&
		    actions[vector][i].arg == arg) {
			release(&irq_lock);
			return -1;
		}
		if (!actions[vector][i].handler && free_slot < 0)
			free_slot = i;
	}
	if (free_slot < 0) {
		release(&irq_lock);
		return -1;
	}
	actions[vector][free_slot] = (struct irq_action){handler, arg};
	release(&irq_lock);
	return 0;
}

int irq_dispatch(uint32 vector)
{
	if (vector >= IRQ_VECTOR_COUNT)
		return 0;
	struct irq_action snapshot[IRQ_SHARED_HANDLERS];
	acquire(&irq_lock);
	for (uint i = 0; i < IRQ_SHARED_HANDLERS; i++)
		snapshot[i] = actions[vector][i];
	release(&irq_lock);
	/* Never hold the registry lock while a driver acquires its own locks.
	 */
	int handled = 0;
	for (uint i = 0; i < IRQ_SHARED_HANDLERS; i++)
		if (snapshot[i].handler)
			handled |= !!snapshot[i].handler(snapshot[i].arg);
	return handled;
}
