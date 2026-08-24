#include "kernel/types.h"
#include "asm/loongarch.h"
#include "platform/uart.h"

extern void kernelvec(void);
void trap_init(void)
{
	w_eentry((uint64) kernelvec);

	// w_ecfg(ECFG_VS(0));
}

void loong_early_boot(void)
{
	trap_init();
	uart_puts("\nFrostVista LoongArch kernel started\n");
	for (;;)
		;
}
