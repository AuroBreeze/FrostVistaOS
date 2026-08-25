#ifndef __KERNEL_ARCH_IRQ_H
#define __KERNEL_ARCH_IRQ_H

#include "asm/irq.h"

static inline void arch_irq_enable(void)
{
	irq_enable();
}

static inline void arch_irq_disable(void)
{
	irq_disable();
}

static inline int arch_irq_save(void)
{
	return irq_save();
}

static inline void arch_irq_restore(int enabled)
{
	irq_restore(enabled);
}

static inline int arch_irq_enabled(void)
{
	return irq_enabled();
}

#endif
