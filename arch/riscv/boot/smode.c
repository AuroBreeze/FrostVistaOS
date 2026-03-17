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
#include "core/proc.h"

void display_banner(void) {
  LOG_INFO("    ______                __ _    ___      __       ");
  LOG_INFO("   / ____/________  _____/ /| |  / (_)____/ /_____ _");
  LOG_INFO("  / /_  / ___/ __ \\/ ___/ __/ | / / / ___/ __/ __ `/");
  LOG_INFO(" / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / ");
  LOG_INFO("/_/   /_/   \\____/____/\\__/ |___/_/____/\\__/\\__,_/");
}

int early_mode = 1;

void __attribute__((noreturn)) high_mode_start() {
  LOG_TRACE("Successfully jumped to high address!");
  LOG_TRACE("Current CPUID: %d", cpuid());

  trapinit();
  uint64 current_sp;
  asm volatile("mv %0, sp" : "=r"(current_sp));
  LOG_TRACE("current_sp: %p", current_sp);

  kalloc_init(); // get memory

  clear_low_memory_mappings();
  LOG_INFO("Hello FrostVista OS!");
  
  procinit();
  user_init();
  scheduler();

  // main();
  while (1) {
  }
}

void s_mode_start() {
  trapinit();

  plic_init_uart();
  pre_uart_init();

  uart_init();

  display_banner();
  LOG_INFO("FrostVistaOS booting...");

  timerinit();

  kvminit();
  kvminithart();

  early_mode = 0;

  uart_base_ptr = (volatile unsigned char *)ADR2HIGH(UART0_BASE);

  uint64 target = (uint64)high_mode_start + KERNEL_VIRT_OFFSET;
  switch_to_high_address(target, KERNEL_VIRT_OFFSET);

  while (1) {
  }
}
