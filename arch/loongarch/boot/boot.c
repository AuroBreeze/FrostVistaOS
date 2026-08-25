#include "kernel/types.h"
#include "asm/loongarch.h"
#include "platform/timer.h"
#include "platform/uart.h"
#include "asm/trap.h"

extern void kernelvec(void);
void trap_init(void)
{
	// EENTRY 保存普通例外和中断的入口基地址。
	w_eentry((uint64) kernelvec);

	// VS=0：所有普通例外和中断共用 EENTRY，由软件读取 ESTAT 分发。
	w_ecfg(ECFG_VS(0));
}

static __attribute__((noinline)) void trigger_store_exception(void)
{
	asm volatile("li.d $t0, 0x0ffff000\n"
		     "st.d $zero, $t0, 0\n"
		     :
		     :
		     : "t0", "memory");
}

void loong_early_boot(void)
{
	trap_init();
	uart_init();
	uart_puts("\nFrostVista LoongArch kernel started\n");

	uint64 id = r_cpuid();
	kprintf("cpuid: %x\n", id);

	timer_init();

	// Test Exception
	// *(volatile uint64 *) 0 = 1; // brk

	// uart_puts("\nbefore store exception\n");
	// trigger_store_exception();
	// uart_puts("\nafter store exception\n");

	for (;;)
		;
}
