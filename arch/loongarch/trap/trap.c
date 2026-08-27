#include "asm/trap.h"
#include "asm/loongarch.h"
#include "kernel/log.h"
#include "kernel/types.h"

#define ESTAT_IS_TIMER (1ULL << 11)

static __attribute__((noreturn)) void trap_halt(void)
{
	for (;;) {
		asm volatile("idle 0");
	}
}

void trap_handler(void)
{
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
		return;
	}

	uint64 ecode = estat_ecode(estat);
	uint64 esubcode = estat_esubcode(estat);
	kprintf("ecode: 0x%x\n", ecode);
	kprintf("esubcode: 0x%x\n", esubcode);
	kprintf("Exception\n");

	/* 未实现的异常不能直接 ertn，否则会重新执行同一条故障指令。 */
	trap_halt();
}
