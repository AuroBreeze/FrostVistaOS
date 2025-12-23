#include "driver/clint.h"
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

void main();

int early_mode = 1;
void *high_adr = kalloc_init + KERNEL_VIRT_OFFSET;

extern pagetable_t kernel_table;
extern char _kernel_end[];
extern char
    *ekalloc_ptr; // Memory addresses used in the initial memory allocation

extern void kernelvec(void);

void __attribute__((noreturn)) high_mode_start() {
  // kprintf("Successfully jumped to high address!\n");
  // 在跳转后的高地址函数里
  // uint64 current_sp;
  // asm volatile("mv %0, sp" : "=r"(current_sp));
  // kprintf("Current SP: %p\n", current_sp);
  w_stvec((uint64)kernelvec);
  kalloc_init(); // get memory
  // kprintf("KERNEL BASE: %p\n", (void *)KERNEL_BASE_LOW);
  // kprintf("PHYSTOP: %p\n", (void *)PHYSTOP_LOW);
  uint64 low_kernel_end = (uint64)_kernel_end;
  if (IS_ADR_HIGHT(_kernel_end)) {
    low_kernel_end = ADR2LOW(_kernel_end);
  }
  kvmunmap(kernel_table, KERNEL_BASE_LOW, (low_kernel_end - KERNEL_BASE_LOW),
           0);
  kvmunmap(kernel_table, (uint64)ekalloc_ptr,
           (PHYSTOP_LOW - (uint64)ekalloc_ptr), 0);

  kprintf("Enable time interrupts...\n");
  w_sie(r_sie() | SIE_SSIE);
  w_sstatus(r_sstatus() | SSTATUS_SIE);
  kprintf("Enabled\n");

  kprintf("Hello FrostVista OS!\n");

  main();

  while (1) {
  }
}

void s_mode_start() {
  w_stvec((uint64)kernelvec);

  uart_init();
  display_banner();
  kprintf("FrostVistaOS booting...\n");

  kvminit();
  kvminithart();
  early_mode = 0;

  uint64 target = (uint64)high_mode_start + KERNEL_VIRT_OFFSET;
  switch_to_high_address(target, KERNEL_VIRT_OFFSET);

  while (1) {
  }
}
