#include "kernel/defs.h"
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

extern void kernelvec(void);
void s_mode_start() {
  w_stvec((uint64)kernelvec);

  uart_init();
  display_banner();
  ekalloc_init(); // Allocate EPAGE pages to provide the necessary ekalloc functionality
  kvminit();
  kvminithart();

  early_mode = 0;
  kalloc_init(); // complete memory allocation after paging
  kprintf("FrostVistaOS booting...\n");
  kprintf("Hello FrostVista OS!\n");
  kprintf("uart_init ok, x=%d, ptr=%p\n", 42, main);
  main();
  return;
}
