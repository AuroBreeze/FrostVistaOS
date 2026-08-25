#ifndef __LOONGARCH_ASM_IRQ_H
#define __LOONGARCH_ASM_IRQ_H

#include "asm/loongarch.h"

// CRMD.IE: global interrupt enable for the current privilege level.
#define CRMD_IE (1ULL << 2)

static inline void irq_enable(void)
{
	w_crmd(r_crmd() | CRMD_IE);
}

static inline void irq_disable(void)
{
	w_crmd(r_crmd() & ~CRMD_IE);
}

static inline int irq_save(void)
{
	int enabled = (r_crmd() & CRMD_IE) != 0;
	irq_disable();
	return enabled;
}

static inline void irq_restore(int enabled)
{
	if (enabled)
		irq_enable();
	else
		irq_disable();
}

static inline int irq_enabled(void)
{
	return (r_crmd() & CRMD_IE) != 0;
}

#endif
