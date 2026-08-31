#include "asm/mm.h"
#include "kernel/types.h"
#include "platform/power.h"

void arch_shutdown(void)
{
	volatile uint8 *power =
	    (volatile uint8 *) (GED_POWER_PAGE_VA + GED_POWER_OFFSET);
	*power = GED_POWER_VALUE;

	for (;;) {
		asm volatile("idle 0");
	}
}
