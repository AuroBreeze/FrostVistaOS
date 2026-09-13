#include "platform/timer.h"
#include "asm/loongarch.h"
#include "kernel/types.h"

void timer_init()
{
	// Convert the target interrupt rate into constant-frequency timer ticks.
	uint64 ticks = TIMER_FREQ / TIMER_HZ;

	uint64 ecfg = r_ecfg();
	uint64 crmd = r_crmd();
	// ECFG.LIE[11] enables the per-core timer interrupt.
	uint64 ecfg_enable = 1 << 11;
	// CRMD.IE[2] is the global interrupt-enable bit for PLV0.
	uint64 crmd_ie = 1 << 2;

	w_ecfg(ecfg | ecfg_enable);
	w_crmd(crmd | crmd_ie);

	// Start the timer in periodic mode and reload ticks when it reaches zero.
	w_tcfg(TCFG_INITVAL(ticks) | TCFG_PERIODIE | TCFG_ENABLE);
}
