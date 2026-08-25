#include "asm/trap.h"
#include "asm/loongarch.h"
#include "kernel/types.h"
#include "platform/uart.h"

void trap_handler(void)
{
	uart_puts("entry trap\n");
	uint64 estat = r_estat();
	if (is_interrupt(estat)) {
		uint64 badv = r_badv();
		kprintf("badv: %x\n", badv);
		kprintf("Interrupt\n");
	} else {
		kprintf("Exception\n");
	}

	uart_puts("test\n");
	for (;;)
		;
}
