#ifndef __RISCV_ASM_IRQ_H
#define __RISCV_ASM_IRQ_H

#include "asm/riscv.h"

#define SSTATUS_SIE (1UL << 1) // Supervisor Interrupt Enable

static inline void irq_enable(void)
{
	w_sstatus(r_sstatus() | SSTATUS_SIE);
}

static inline void irq_disable(void)
{
	w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

static inline int irq_save(void)
{
	int enabled = (r_sstatus() & SSTATUS_SIE) != 0;
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
	return (r_sstatus() & SSTATUS_SIE) != 0;
}

#endif
