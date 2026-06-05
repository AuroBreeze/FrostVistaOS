#include "asm/smode.h"
#include "asm/defs.h"
#include "asm/machine.h"
#include "asm/mm.h"
#include "asm/riscv.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "platform/defs.h"
#include "platform/uart.h"

#define LOG_MODULE "BOOT"

void display_banner(void)
{
	LOG_SEP();
	LOG_BANNER("    ______                __ _    ___      __       ");
	LOG_BANNER("   / ____/________  _____/ /| |  / (_)____/ /_____ _");
	LOG_BANNER("  / /_  / ___/ __ \\/ ___/ __/ | / / / ___/ __/ __ `/");
	LOG_BANNER(" / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / ");
	LOG_BANNER("/_/   /_/   \\____/____/\\__/ |___/_/____/\\__/\\__,_/");
	LOG_BANNER("");
	LOG_BANNER("RISC-V 64  |  Sv39  |  v1.0");
	LOG_SEP();
}

int early_mode = 1;

void __attribute__((noreturn)) high_mode_start()
{
	LOG_TRACE("Successfully jumped to high address!");
	LOG_TRACE("Current CPUID: %d", cpuid());

	LOG_PHASE("Platform Init");

	trapinit();
	{
		uint64 current_sp;
		asm volatile("mv %0, sp" : "=r"(current_sp));
		LOG_TRACE("current_sp: %p", current_sp);
	}
	kalloc_init();
	clear_low_memory_mappings();
	LOG_INFO("Hello FrostVista OS!");

	LOG_PHASE("Process Subsystem");
	procinit();

	LOG_PHASE("Filesystem & Devices");
	vfs_init();
	virtio_disk_init();
	binit();
	icache_init();

	LOG_PHASE("Kernel Ready");
	user_init();
	scheduler();
	while (1) {
	}
}

void s_mode_start()
{
	trapinit();

	// FIXME: The current system's UART still uses UART output logging
	// before initialization. Although this does not cause any issues under
	// -O2 optimization, it still needs to be fixed.
	pre_uart_init();
	uart_init();

	kvminit();
	kvminithart();

	plic_init_uart();

	display_banner();

	timerinit();

	// TODO: Using macro definitions to resolve magic numbers
	early_mode = 0;

	uart_base_ptr = (volatile unsigned char *) PA2VA(UART0_BASE);

	// NOTE: The virtual address of `kernel_table` will be used later
	// However, it is important to note that writing to the page table still
	// requires a real physical address, which means it must be converted to
	// a low address.
	kernel_table = (pagetable_t) PA2VA((uint64) kernel_table);

	LOG_TRACE("kernel_table: %p", kernel_table);

	uint64 target = (uint64) high_mode_start + KERNEL_VIRT_OFFSET;
	switch_to_high_address(target, KERNEL_VIRT_OFFSET);

	while (1) {
	}
}
