#include "kernel/smode.h"
#include "kernel/defs.h"
#include "kernel/kalloc.h"
#include "kernel/machine.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

void display_banner(void) {
  uart_puts("    ______                __ _    ___      __       \n");
  uart_puts("   / ____/________  _____/ /| |  / (_)____/ /_____ _\n");
  uart_puts("  / /_  / ___/ __ \\/ ___/ __/ | / / / ___/ __/ __ `/\n");
  uart_puts(" / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / \n");
  uart_puts("/_/   /_/   \\____/____/\\__/ |___/_/____/\\__/\\__,_/  \n");
}

int early_mode = 1;

void clear_low_memory_mappings() {
  uint64 low_kernel_end = (uint64)_kernel_end;
  if (IS_ADR_HIGHT(_kernel_end)) {
    low_kernel_end = ADR2LOW(_kernel_end);
  }
  kvmunmap(kernel_table, KERNEL_BASE_LOW, (low_kernel_end - KERNEL_BASE_LOW),
           0);
  kvmunmap(kernel_table, (uint64)ekalloc_ptr,
           (PHYSTOP_LOW - (uint64)ekalloc_ptr), 0);
  sfence_vma();
}

void __attribute__((noreturn)) high_mode_start() {
  kprintf("Successfully jumped to high address!\n");
  kprintf("Current CUPID: %d\n", cupid());
  trapinit();
  uint64 current_sp;
  asm volatile("mv %0, sp" : "=r"(current_sp));
  kprintf("Current SP: %p\n", current_sp);
  kalloc_init(); // get memory

  clear_low_memory_mappings();

  kprintf("Hello FrostVista OS!\n");

  main();

  while (1) {
  }
}

void s_mode_start() {
  trapinit();

  plic_init_uart();
  pre_uart_init();
  uart_init();

  display_banner();
  kprintf("FrostVistaOS booting...\n");

  timerinit();

  kvminit();
  kvminithart();
  early_mode = 0;

  uint64 target = (uint64)high_mode_start + KERNEL_VIRT_OFFSET;
  switch_to_high_address(target, KERNEL_VIRT_OFFSET);

  while (1) {
  }
}
