#include "asm/trap.h"
#include "asm/loongarch.h"
#include "kernel/log.h"
#include "kernel/types.h"

#define ESTAT_IS_TIMER (1ULL << 11)

void trap_handler(void)
{
	// kprintf("Entry trap\n");
	kprintf("Entry trap\n");

	uint64 estat = r_estat();
	if (is_interrupt(estat)) {
		uint64 is = estat_is(estat);
		if (is & ESTAT_IS_TIMER) {
			w_ticlr(1);
			kprintf("Timer\n");
			return;
		}
		kprintf("is: 0x%x\n", is);
		kprintf("Interrupt\n");
	} else {
		uint64 ecode = estat_ecode(estat);
		if (ecode == 0x8) {
			uint64 esubcode = estat_esubcode(estat);
			kprintf("esubcode: 0x%x\n", esubcode);
		}
		if (ecode == -0xB) {
			w_era(r_era() + 4);
		}

		kprintf("ecode: 0x%x\n", ecode);
		kprintf("Exception\n");
	}

	w_era(r_era() + 4);
	return;
}
